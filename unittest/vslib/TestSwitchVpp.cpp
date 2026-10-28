#include "MockVppXlate.h"
#include "vpp/EventPayloadVppEvent.h"
#include "vpp/SwitchVppUtils.h"
#include "vpp/vppxlate/SaiIntfStats.h"
#include "vpp/vppxlate/SaiVppXlate.h"
#include "ContextConfigContainer.h"
#include "EventPayloadNotification.h"
#include "HostInterfaceInfo.h"
#include "SwitchConfigContainer.h"
#include "meta/DummySaiInterface.h"
#include "meta/Meta.h"
#include "meta/sai_serialize.h"
#include "saivs.h"

#include <gtest/gtest.h>

#include <arpa/inet.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <set>
#include <vector>
#include <unistd.h>

using namespace saivs;

TEST(SwitchVpp, getLagMemberEgressDisableAction)
{
    using Action = SwitchVpp::LagMemberEgressDisableAction;

    EXPECT_EQ(Action::NONE, SwitchVpp::getLagMemberEgressDisableAction(false, true, false));
    EXPECT_EQ(Action::DISABLE, SwitchVpp::getLagMemberEgressDisableAction(true, true, false));
    EXPECT_EQ(Action::ENABLE, SwitchVpp::getLagMemberEgressDisableAction(false, true, true));
    EXPECT_EQ(Action::NONE, SwitchVpp::getLagMemberEgressDisableAction(true, true, true));
    EXPECT_EQ(Action::NONE, SwitchVpp::getLagMemberEgressDisableAction(false, false, false));
    EXPECT_EQ(Action::DISABLE, SwitchVpp::getLagMemberEgressDisableAction(true, false, false));
}

class SwitchVppNeighborMissTrap : public ::testing::Test
{
    protected:

        void SetUp() override
        {
            vppMock().reset();
            auto sc = std::make_shared<SwitchConfig>(0, "");

            sc->m_saiSwitchType = SAI_SWITCH_TYPE_NPU;
            sc->m_switchType = SAI_VS_SWITCH_TYPE_VPP;
            sc->m_bootType = SAI_VS_BOOT_TYPE_COLD;
            sc->m_useTapDevice = false;
            sc->m_profileMap[SAI_KEY_VS_PORT_CONFIG_FILE] = "/dev/null";
            sc->m_laneMap = LaneMap::getDefaultLaneMap(0);
            sc->m_eventQueue = std::make_shared<EventQueue>(std::make_shared<Signal>());

            auto scc = std::make_shared<SwitchConfigContainer>();
            scc->insert(sc);

            m_mgr = std::make_shared<RealObjectIdManager>(0, scc);
            m_sw = std::make_shared<TestableSwitchVpp>(m_switchId, m_mgr, sc);
        }

        const sai_object_id_t m_switchId = 0x2100000000;
        std::shared_ptr<RealObjectIdManager> m_mgr;
        std::shared_ptr<TestableSwitchVpp> m_sw;
};

TEST_F(SwitchVppNeighborMissTrap, BindsDhcpFamiliesToTheirConfiguredPolicers)
{
    const sai_object_id_t v4Group = 0x1200000010;
    const sai_object_id_t v6Group = 0x1200000011;
    const sai_object_id_t v4Policer = 0x1200000012;
    const sai_object_id_t v6Policer = 0x1200000013;

    vpp_trap_group_entry_t group{};
    group.admin_state = true;
    group.policer_oid = v4Policer;
    m_sw->m_trap_group_map[v4Group] = group;
    group.policer_oid = v6Policer;
    m_sw->m_trap_group_map[v6Group] = group;

    vpp_trap_entry_t v4{};
    v4.trap_type = SAI_HOSTIF_TRAP_TYPE_DHCP;
    v4.packet_action = SAI_PACKET_ACTION_TRAP;
    v4.trap_group_oid = v4Group;
    vpp_trap_entry_t v6 = v4;
    v6.trap_type = SAI_HOSTIF_TRAP_TYPE_DHCPV6;
    v6.trap_group_oid = v6Group;

    ASSERT_EQ(SAI_STATUS_SUCCESS,
            m_sw->installTrapClassifyNow(0x1200000014, v4, 12));
    ASSERT_EQ(SAI_STATUS_SUCCESS,
            m_sw->installTrapClassifyNow(0x1200000015, v6, 13));
    ASSERT_EQ(2u, vppMock().coppDhcpBindCalls.size());

    EXPECT_EQ("copp-policer-0x1200000012",
            vppMock().coppDhcpBindCalls[0].policerName);
    EXPECT_FALSE(vppMock().coppDhcpBindCalls[0].isIpv6);
    EXPECT_TRUE(vppMock().coppDhcpBindCalls[0].isBind);
    EXPECT_EQ("copp-policer-0x1200000013",
            vppMock().coppDhcpBindCalls[1].policerName);
    EXPECT_TRUE(vppMock().coppDhcpBindCalls[1].isIpv6);
    EXPECT_FALSE(vppMock().coppDhcpBindCalls[1].isCopy);
    EXPECT_TRUE(vppMock().coppDhcpBindCalls[1].isBind);

    ASSERT_EQ(SAI_STATUS_SUCCESS,
            m_sw->uninstallTrapClassifyNow(0x1200000014, v4));
    ASSERT_EQ(3u, vppMock().coppDhcpBindCalls.size());
    EXPECT_FALSE(vppMock().coppDhcpBindCalls[2].isIpv6);
    EXPECT_FALSE(vppMock().coppDhcpBindCalls[2].isBind);
}

TEST_F(SwitchVppNeighborMissTrap, DhcpBindFailureCleansTheAttemptedOwner)
{
    const sai_object_id_t groupOid = 0x1200000020;
    const sai_object_id_t policerOid = 0x1200000021;

    vpp_trap_group_entry_t group{};
    group.admin_state = true;
    group.policer_oid = policerOid;
    m_sw->m_trap_group_map[groupOid] = group;

    vpp_trap_entry_t trap{};
    trap.trap_type = SAI_HOSTIF_TRAP_TYPE_DHCP;
    trap.packet_action = SAI_PACKET_ACTION_TRAP;
    trap.trap_group_oid = groupOid;

    vppMock().coppDhcpBindFailOnCall = 1;
    EXPECT_EQ(SAI_STATUS_FAILURE,
            m_sw->installTrapClassifyNow(0x1200000022, trap, 21));
    ASSERT_EQ(3u, vppMock().coppDhcpBindCalls.size());
    EXPECT_TRUE(vppMock().coppDhcpBindCalls[0].isBind);
    EXPECT_FALSE(vppMock().coppDhcpBindCalls[1].isBind);
    EXPECT_EQ("copp-policer-0x1200000021",
            vppMock().coppDhcpBindCalls[1].policerName);
    // The physical snapshot was absent, not present+NULL. Reconcile that
    // tri-state explicitly after the owner-qualified timeout cleanup.
    EXPECT_TRUE(vppMock().coppDhcpBindCalls[2].policerName.empty());
    EXPECT_FALSE(vppMock().coppDhcpBindCalls[2].isBind);
}

TEST_F(SwitchVppNeighborMissTrap, DhcpNullPolicerKeepsTheTrapActiveUnpoliced)
{
    const sai_object_id_t groupOid = 0x1200000023;
    const sai_object_id_t trapOid = 0x1200000024;

    vpp_trap_group_entry_t group{};
    group.admin_state = true;
    group.policer_oid = SAI_NULL_OBJECT_ID;
    m_sw->m_trap_group_map[groupOid] = group;

    vpp_trap_entry_t trap{};
    trap.trap_type = SAI_HOSTIF_TRAP_TYPE_DHCP;
    trap.packet_action = SAI_PACKET_ACTION_TRAP;
    trap.trap_group_oid = groupOid;

    ASSERT_EQ(SAI_STATUS_SUCCESS,
            m_sw->installTrapClassifyNow(trapOid, trap, (uint32_t)~0));
    ASSERT_EQ(1u, vppMock().coppDhcpBindCalls.size());
    EXPECT_TRUE(vppMock().coppDhcpBindCalls[0].policerName.empty());
    EXPECT_TRUE(vppMock().coppDhcpBindCalls[0].isBind);
    EXPECT_FALSE(vppMock().coppDhcpBindCalls[0].isIpv6);
    EXPECT_FALSE(vppMock().coppDhcpBindCalls[0].isCopy);

    ASSERT_EQ(SAI_STATUS_SUCCESS,
            m_sw->uninstallTrapClassifyNow(trapOid, trap));
    ASSERT_EQ(2u, vppMock().coppDhcpBindCalls.size());
    EXPECT_TRUE(vppMock().coppDhcpBindCalls[1].policerName.empty());
    EXPECT_FALSE(vppMock().coppDhcpBindCalls[1].isBind);
}

TEST_F(SwitchVppNeighborMissTrap, DhcpFailedReplacementRestoresUnpolicedState)
{
    const sai_object_id_t groupOid = 0x1200000025;
    const sai_object_id_t policerOid = 0x1200000026;
    const sai_object_id_t trapOid = 0x1200000027;

    vpp_trap_group_entry_t group{};
    group.admin_state = true;
    group.policer_oid = SAI_NULL_OBJECT_ID;
    m_sw->m_trap_group_map[groupOid] = group;

    vpp_trap_entry_t trap{};
    trap.trap_type = SAI_HOSTIF_TRAP_TYPE_DHCPV6;
    trap.packet_action = SAI_PACKET_ACTION_COPY;
    trap.trap_group_oid = groupOid;
    trap.classify_installed = true;

    ASSERT_EQ(SAI_STATUS_SUCCESS,
            m_sw->installTrapClassifyNow(trapOid, trap, (uint32_t)~0));
    m_sw->m_trap_map[trapOid] = trap;
    vppMock().coppDhcpBindCalls.clear();

    group.policer_oid = policerOid;
    m_sw->m_trap_group_map[groupOid] = group;
    vppMock().coppDhcpBindFailOnCall = 1;
    EXPECT_EQ(SAI_STATUS_FAILURE,
            m_sw->installTrapClassifyNow(trapOid, trap, 26, policerOid));

    ASSERT_EQ(3u, vppMock().coppDhcpBindCalls.size());
    EXPECT_EQ("copp-policer-0x1200000026",
            vppMock().coppDhcpBindCalls[0].policerName);
    EXPECT_TRUE(vppMock().coppDhcpBindCalls[0].isBind);
    EXPECT_TRUE(vppMock().coppDhcpBindCalls[0].isCopy);
    EXPECT_FALSE(vppMock().coppDhcpBindCalls[1].isBind);
    EXPECT_TRUE(vppMock().coppDhcpBindCalls[2].policerName.empty());
    EXPECT_TRUE(vppMock().coppDhcpBindCalls[2].isBind);
    EXPECT_TRUE(vppMock().coppDhcpBindCalls[2].isIpv6);
    EXPECT_TRUE(vppMock().coppDhcpBindCalls[2].isCopy);
}

TEST_F(SwitchVppNeighborMissTrap, DhcpRemoveFailureRestoresPhysicalAndMapState)
{
    const sai_object_id_t groupOid = 0x1200000028;
    const sai_object_id_t policerOid = 0x1200000029;
    const sai_object_id_t trapOid = 0x120000002a;

    vpp_trap_group_entry_t group{};
    group.admin_state = true;
    group.policer_oid = policerOid;
    m_sw->m_trap_group_map[groupOid] = group;

    vpp_policer_entry_t policer{};
    policer.vpp_policer_index = 29;
    m_sw->m_policer_map[policerOid] = policer;

    vpp_trap_entry_t trap{};
    trap.trap_type = SAI_HOSTIF_TRAP_TYPE_DHCP;
    trap.packet_action = SAI_PACKET_ACTION_TRAP;
    trap.trap_group_oid = groupOid;
    trap.classify_installed = true;

    ASSERT_EQ(SAI_STATUS_SUCCESS,
            m_sw->installTrapClassifyNow(trapOid, trap, 29, policerOid));
    m_sw->m_trap_map[trapOid] = trap;
    vppMock().coppDhcpBindCalls.clear();

    m_sw->removeInternalResult = SAI_STATUS_FAILURE;
    EXPECT_EQ(SAI_STATUS_FAILURE,
            m_sw->removeHostifTrap(sai_serialize_object_id(trapOid)));

    ASSERT_EQ(2u, vppMock().coppDhcpBindCalls.size());
    EXPECT_FALSE(vppMock().coppDhcpBindCalls[0].isBind);
    EXPECT_EQ("copp-policer-0x1200000029",
            vppMock().coppDhcpBindCalls[1].policerName);
    EXPECT_TRUE(vppMock().coppDhcpBindCalls[1].isBind);
    EXPECT_EQ(1u, m_sw->m_trap_map.count(trapOid));
    EXPECT_EQ(policerOid, m_sw->m_trap_binding_map.at(0x10003).policer_oid);
}

TEST_F(SwitchVppNeighborMissTrap, DhcpFailedReplacementRestoresOldOwner)
{
    const sai_object_id_t groupOid = 0x1200000030;
    const sai_object_id_t oldPolicer = 0x1200000031;
    const sai_object_id_t newPolicer = 0x1200000032;

    vpp_trap_group_entry_t group{};
    group.admin_state = true;
    group.policer_oid = oldPolicer;
    m_sw->m_trap_group_map[groupOid] = group;

    vpp_policer_entry_t oldEntry{};
    oldEntry.vpp_policer_index = 31;
    m_sw->m_policer_map[oldPolicer] = oldEntry;
    vpp_policer_entry_t newEntry{};
    newEntry.vpp_policer_index = 32;
    m_sw->m_policer_map[newPolicer] = newEntry;

    vpp_trap_entry_t trap{};
    trap.trap_type = SAI_HOSTIF_TRAP_TYPE_DHCPV6;
    trap.packet_action = SAI_PACKET_ACTION_TRAP;
    trap.trap_group_oid = groupOid;
    const sai_object_id_t trapOid = 0x1200000033;

    ASSERT_EQ(SAI_STATUS_SUCCESS,
            m_sw->installTrapClassifyNow(trapOid, trap, 31));
    vppMock().coppDhcpBindCalls.clear();

    group.policer_oid = newPolicer;
    m_sw->m_trap_group_map[groupOid] = group;
    vppMock().coppDhcpBindFailOnCall = 1;
    EXPECT_EQ(SAI_STATUS_FAILURE,
            m_sw->installTrapClassifyNow(trapOid, trap, 32));

    ASSERT_EQ(3u, vppMock().coppDhcpBindCalls.size());
    EXPECT_EQ("copp-policer-0x1200000032",
            vppMock().coppDhcpBindCalls[0].policerName);
    EXPECT_TRUE(vppMock().coppDhcpBindCalls[0].isBind);
    EXPECT_FALSE(vppMock().coppDhcpBindCalls[1].isBind);
    EXPECT_EQ("copp-policer-0x1200000031",
            vppMock().coppDhcpBindCalls[2].policerName);
    EXPECT_TRUE(vppMock().coppDhcpBindCalls[2].isBind);
    EXPECT_TRUE(vppMock().coppDhcpBindCalls[2].isIpv6);
}

TEST_F(SwitchVppNeighborMissTrap, DhcpUnresolvedRequestedPolicerDoesNotUseAnotherOwner)
{
    const sai_object_id_t oldGroup = 0x1200000040;
    const sai_object_id_t oldPolicer = 0x1200000041;
    const sai_object_id_t oldTrapOid = 0x1200000042;
    const sai_object_id_t newGroup = 0x1200000043;
    const sai_object_id_t unresolvedPolicer = 0x1200000044;
    const sai_object_id_t newTrapOid = 0x1200000045;

    vpp_trap_group_entry_t group{};
    group.policer_oid = oldPolicer;
    m_sw->m_trap_group_map[oldGroup] = group;
    group.policer_oid = unresolvedPolicer;
    m_sw->m_trap_group_map[newGroup] = group;

    vpp_policer_entry_t installed{};
    installed.vpp_policer_index = 41;
    m_sw->m_policer_map[oldPolicer] = installed;

    vpp_trap_entry_t oldTrap{};
    oldTrap.trap_type = SAI_HOSTIF_TRAP_TYPE_DHCP;
    oldTrap.packet_action = SAI_PACKET_ACTION_TRAP;
    oldTrap.trap_group_oid = oldGroup;
    oldTrap.classify_installed = true;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->installTrapClassifyNow(
            oldTrapOid, oldTrap, 41, oldPolicer));
    m_sw->m_trap_map[oldTrapOid] = oldTrap;

    vpp_trap_entry_t newTrap = oldTrap;
    newTrap.trap_group_oid = newGroup;
    newTrap.classify_installed = false;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->installTrapClassifyNow(
            newTrapOid, newTrap, (uint32_t)~0, unresolvedPolicer));

    ASSERT_EQ(2u, vppMock().coppDhcpBindCalls.size());
    EXPECT_EQ("copp-policer-0x1200000044",
            vppMock().coppDhcpBindCalls[1].policerName);
    EXPECT_EQ(unresolvedPolicer,
            m_sw->m_trap_binding_map.at(0x10003).policer_oid);
}

TEST_F(SwitchVppNeighborMissTrap, DhcpSharedOwnersKeepTrapUntilAllAreCopy)
{
    const sai_object_id_t groupOid = 0x1200000050;
    const sai_object_id_t policerOid = 0x1200000051;
    const sai_object_id_t trapOid = 0x1200000052;
    const sai_object_id_t copyOid = 0x1200000053;

    vpp_trap_group_entry_t group{};
    group.policer_oid = policerOid;
    m_sw->m_trap_group_map[groupOid] = group;

    vpp_policer_entry_t policer{};
    policer.vpp_policer_index = 51;
    m_sw->m_policer_map[policerOid] = policer;

    vpp_trap_entry_t trap{};
    trap.trap_type = SAI_HOSTIF_TRAP_TYPE_DHCPV6;
    trap.packet_action = SAI_PACKET_ACTION_TRAP;
    trap.trap_group_oid = groupOid;
    trap.classify_installed = true;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->installTrapClassifyNow(
            trapOid, trap, 51, policerOid));
    m_sw->m_trap_map[trapOid] = trap;

    vpp_trap_entry_t copy = trap;
    copy.packet_action = SAI_PACKET_ACTION_COPY;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->installTrapClassifyNow(
            copyOid, copy, 51, policerOid));
    copy.classify_installed = true;
    m_sw->m_trap_map[copyOid] = copy;

    ASSERT_EQ(2u, vppMock().coppDhcpBindCalls.size());
    EXPECT_FALSE(vppMock().coppDhcpBindCalls[0].isCopy);
    EXPECT_FALSE(vppMock().coppDhcpBindCalls[1].isCopy);

    ASSERT_EQ(SAI_STATUS_SUCCESS,
            m_sw->uninstallTrapClassifyNow(trapOid, trap));
    ASSERT_EQ(3u, vppMock().coppDhcpBindCalls.size());
    EXPECT_TRUE(vppMock().coppDhcpBindCalls[2].isBind);
    EXPECT_TRUE(vppMock().coppDhcpBindCalls[2].isCopy);
    EXPECT_EQ(SAI_PACKET_ACTION_COPY,
            m_sw->m_trap_binding_map.at(0x10004).packet_action);
}

TEST_F(SwitchVppNeighborMissTrap, DhcpQueueUpdateDoesNotStealSharedPolicer)
{
    const sai_object_id_t oldGroup = 0x1200000054;
    const sai_object_id_t newGroup = 0x1200000055;
    const sai_object_id_t oldPolicer = 0x1200000056;
    const sai_object_id_t newPolicer = 0x1200000057;
    const sai_object_id_t oldTrapOid = 0x1200000058;
    const sai_object_id_t newTrapOid = 0x1200000059;

    sai_attribute_t groupAttr{};
    groupAttr.id = SAI_HOSTIF_TRAP_GROUP_ATTR_POLICER;
    groupAttr.value.oid = oldPolicer;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrapGroup(
            oldGroup, m_switchId, 1, &groupAttr));
    groupAttr.value.oid = newPolicer;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrapGroup(
            newGroup, m_switchId, 1, &groupAttr));

    vpp_policer_entry_t policer{};
    policer.vpp_policer_index = 56;
    m_sw->m_policer_map[oldPolicer] = policer;
    policer.vpp_policer_index = 57;
    m_sw->m_policer_map[newPolicer] = policer;

    sai_attribute_t trapAttrs[3]{};
    trapAttrs[0].id = SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE;
    trapAttrs[0].value.s32 = SAI_HOSTIF_TRAP_TYPE_DHCP;
    trapAttrs[1].id = SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION;
    trapAttrs[1].value.s32 = SAI_PACKET_ACTION_TRAP;
    trapAttrs[2].id = SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP;
    trapAttrs[2].value.oid = oldGroup;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrap(
            oldTrapOid, m_switchId, 3, trapAttrs));
    trapAttrs[2].value.oid = newGroup;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrap(
            newTrapOid, m_switchId, 3, trapAttrs));
    ASSERT_EQ(newPolicer,
            m_sw->m_trap_binding_map.at(0x10003).policer_oid);
    vppMock().coppDhcpBindCalls.clear();

    sai_attribute_t queueAttr{};
    queueAttr.id = SAI_HOSTIF_TRAP_GROUP_ATTR_QUEUE;
    queueAttr.value.u32 = 7;
    EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->setHostifTrapGroup(
            sai_serialize_object_id(oldGroup), &queueAttr));

    sai_attribute_t adminAttr{};
    adminAttr.id = SAI_HOSTIF_TRAP_GROUP_ATTR_ADMIN_STATE;
    adminAttr.value.booldata = false;
    EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->setHostifTrapGroup(
            sai_serialize_object_id(oldGroup), &adminAttr));

    groupAttr.value.oid = oldPolicer;
    EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->setHostifTrapGroup(
            sai_serialize_object_id(oldGroup), &groupAttr));

    EXPECT_TRUE(vppMock().coppDhcpBindCalls.empty());
    EXPECT_EQ(newPolicer,
            m_sw->m_trap_binding_map.at(0x10003).policer_oid);
    EXPECT_EQ(7u, m_sw->m_trap_group_map.at(oldGroup).queue);
    EXPECT_FALSE(m_sw->m_trap_group_map.at(oldGroup).admin_state);
}

TEST_F(SwitchVppNeighborMissTrap, DhcpActionOnlyUpdatePreservesSharedPolicerOwner)
{
    const sai_object_id_t oldGroup = 0x120000005a;
    const sai_object_id_t newGroup = 0x120000005b;
    const sai_object_id_t oldPolicer = 0x120000005c;
    const sai_object_id_t newPolicer = 0x120000005d;
    const sai_object_id_t oldTrapOid = 0x120000005e;
    const sai_object_id_t newTrapOid = 0x120000005f;

    sai_attribute_t groupAttr{};
    groupAttr.id = SAI_HOSTIF_TRAP_GROUP_ATTR_POLICER;
    groupAttr.value.oid = oldPolicer;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrapGroup(
            oldGroup, m_switchId, 1, &groupAttr));
    groupAttr.value.oid = newPolicer;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrapGroup(
            newGroup, m_switchId, 1, &groupAttr));

    vpp_policer_entry_t policer{};
    policer.vpp_policer_index = 60;
    m_sw->m_policer_map[oldPolicer] = policer;
    policer.vpp_policer_index = 61;
    m_sw->m_policer_map[newPolicer] = policer;

    sai_attribute_t trapAttrs[3]{};
    trapAttrs[0].id = SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE;
    trapAttrs[0].value.s32 = SAI_HOSTIF_TRAP_TYPE_DHCPV6;
    trapAttrs[1].id = SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION;
    trapAttrs[1].value.s32 = SAI_PACKET_ACTION_TRAP;
    trapAttrs[2].id = SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP;
    trapAttrs[2].value.oid = oldGroup;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrap(
            oldTrapOid, m_switchId, 3, trapAttrs));
    trapAttrs[1].value.s32 = SAI_PACKET_ACTION_COPY;
    trapAttrs[2].value.oid = newGroup;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrap(
            newTrapOid, m_switchId, 3, trapAttrs));
    ASSERT_EQ(newPolicer,
            m_sw->m_trap_binding_map.at(0x10004).policer_oid);
    vppMock().coppDhcpBindCalls.clear();

    sai_attribute_t actionAttr{};
    actionAttr.id = SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION;
    actionAttr.value.s32 = SAI_PACKET_ACTION_COPY;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->setHostifTrap(
            sai_serialize_object_id(oldTrapOid), &actionAttr));

    ASSERT_EQ(1u, vppMock().coppDhcpBindCalls.size());
    EXPECT_EQ("copp-policer-0x120000005d",
            vppMock().coppDhcpBindCalls[0].policerName);
    EXPECT_TRUE(vppMock().coppDhcpBindCalls[0].isCopy);
    EXPECT_EQ(newPolicer,
            m_sw->m_trap_binding_map.at(0x10004).policer_oid);
    EXPECT_EQ(SAI_PACKET_ACTION_COPY,
            m_sw->m_trap_binding_map.at(0x10004).packet_action);

    vppMock().coppDhcpBindCalls.clear();
    actionAttr.value.s32 = SAI_PACKET_ACTION_TRAP;
    m_sw->setInternalResult = SAI_STATUS_FAILURE;
    EXPECT_EQ(SAI_STATUS_FAILURE, m_sw->setHostifTrap(
            sai_serialize_object_id(oldTrapOid), &actionAttr));
    ASSERT_EQ(2u, vppMock().coppDhcpBindCalls.size());
    EXPECT_EQ("copp-policer-0x120000005d",
            vppMock().coppDhcpBindCalls[0].policerName);
    EXPECT_EQ("copp-policer-0x120000005d",
            vppMock().coppDhcpBindCalls[1].policerName);
    EXPECT_FALSE(vppMock().coppDhcpBindCalls[0].isCopy);
    EXPECT_TRUE(vppMock().coppDhcpBindCalls[1].isCopy);
    EXPECT_EQ(newPolicer,
            m_sw->m_trap_binding_map.at(0x10004).policer_oid);
    EXPECT_EQ(SAI_PACKET_ACTION_COPY,
            m_sw->m_trap_binding_map.at(0x10004).packet_action);
    EXPECT_EQ(SAI_PACKET_ACTION_COPY,
            m_sw->m_trap_map.at(oldTrapOid).packet_action);
}

TEST_F(SwitchVppNeighborMissTrap, DhcpActionOnlyUpdatePreservesUnpolicedOwner)
{
    const sai_object_id_t policedGroup = 0x120000006a;
    const sai_object_id_t unpolicedGroup = 0x120000006b;
    const sai_object_id_t policerOid = 0x120000006c;
    const sai_object_id_t policedTrapOid = 0x120000006d;
    const sai_object_id_t unpolicedTrapOid = 0x120000006e;

    sai_attribute_t groupAttr{};
    groupAttr.id = SAI_HOSTIF_TRAP_GROUP_ATTR_POLICER;
    groupAttr.value.oid = policerOid;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrapGroup(
            policedGroup, m_switchId, 1, &groupAttr));
    groupAttr.value.oid = SAI_NULL_OBJECT_ID;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrapGroup(
            unpolicedGroup, m_switchId, 1, &groupAttr));

    vpp_policer_entry_t policer{};
    policer.vpp_policer_index = 62;
    m_sw->m_policer_map[policerOid] = policer;

    sai_attribute_t trapAttrs[3]{};
    trapAttrs[0].id = SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE;
    trapAttrs[0].value.s32 = SAI_HOSTIF_TRAP_TYPE_DHCP;
    trapAttrs[1].id = SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION;
    trapAttrs[1].value.s32 = SAI_PACKET_ACTION_COPY;
    trapAttrs[2].id = SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP;
    trapAttrs[2].value.oid = policedGroup;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrap(
            policedTrapOid, m_switchId, 3, trapAttrs));
    trapAttrs[1].value.s32 = SAI_PACKET_ACTION_TRAP;
    trapAttrs[2].value.oid = unpolicedGroup;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrap(
            unpolicedTrapOid, m_switchId, 3, trapAttrs));

    // Model an authoritative present+NULL journal (for example after replay
    // restoration). An action-only update must not reinterpret NULL as a
    // request to select the other owner's P1.
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->installTrapClassifyNow(
            unpolicedTrapOid, m_sw->m_trap_map.at(unpolicedTrapOid),
            (uint32_t)~0, SAI_NULL_OBJECT_ID,
            SAI_NULL_OBJECT_ID, false));
    ASSERT_EQ(SAI_NULL_OBJECT_ID,
            m_sw->m_trap_binding_map.at(0x10003).policer_oid);
    vppMock().coppDhcpBindCalls.clear();

    sai_attribute_t actionAttr{};
    actionAttr.id = SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION;
    actionAttr.value.s32 = SAI_PACKET_ACTION_COPY;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->setHostifTrap(
            sai_serialize_object_id(unpolicedTrapOid), &actionAttr));

    ASSERT_EQ(1u, vppMock().coppDhcpBindCalls.size());
    EXPECT_TRUE(vppMock().coppDhcpBindCalls[0].policerName.empty());
    EXPECT_TRUE(vppMock().coppDhcpBindCalls[0].isBind);
    EXPECT_TRUE(vppMock().coppDhcpBindCalls[0].isCopy);
    EXPECT_EQ(SAI_NULL_OBJECT_ID,
            m_sw->m_trap_binding_map.at(0x10003).policer_oid);
    EXPECT_EQ(SAI_PACKET_ACTION_COPY,
            m_sw->m_trap_binding_map.at(0x10003).packet_action);
}

TEST_F(SwitchVppNeighborMissTrap, DhcpRejectsDropOnCreateAndSet)
{
    const sai_object_id_t groupOid = 0x1200000060;
    const sai_object_id_t trapOid = 0x1200000061;
    const sai_object_id_t rejectedOid = 0x1200000062;

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrapGroup(
            groupOid, m_switchId, 0, nullptr));

    sai_attribute_t attrs[3]{};
    attrs[0].id = SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE;
    attrs[0].value.s32 = SAI_HOSTIF_TRAP_TYPE_DHCP;
    attrs[1].id = SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION;
    attrs[1].value.s32 = SAI_PACKET_ACTION_DROP;
    attrs[2].id = SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP;
    attrs[2].value.oid = groupOid;
    EXPECT_EQ(SAI_STATUS_NOT_SUPPORTED, m_sw->createHostifTrap(
            rejectedOid, m_switchId, 3, attrs));

    attrs[1].value.s32 = SAI_PACKET_ACTION_TRAP;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrap(
            trapOid, m_switchId, 3, attrs));
    vppMock().coppDhcpBindCalls.clear();

    sai_attribute_t setAttr{};
    setAttr.id = SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION;
    setAttr.value.s32 = SAI_PACKET_ACTION_DROP;
    EXPECT_EQ(SAI_STATUS_NOT_SUPPORTED, m_sw->setHostifTrap(
            sai_serialize_object_id(trapOid), &setAttr));
    EXPECT_TRUE(vppMock().coppDhcpBindCalls.empty());
    EXPECT_EQ(SAI_PACKET_ACTION_TRAP,
            m_sw->m_trap_map.at(trapOid).packet_action);
}

TEST_F(SwitchVppNeighborMissTrap, DhcpActionSetReprogramsAndRollbackRestoresAction)
{
    const sai_object_id_t groupOid = 0x1200000070;
    const sai_object_id_t trapOid = 0x1200000071;

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrapGroup(
            groupOid, m_switchId, 0, nullptr));

    sai_attribute_t attrs[3]{};
    attrs[0].id = SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE;
    attrs[0].value.s32 = SAI_HOSTIF_TRAP_TYPE_DHCPV6;
    attrs[1].id = SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION;
    attrs[1].value.s32 = SAI_PACKET_ACTION_COPY;
    attrs[2].id = SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP;
    attrs[2].value.oid = groupOid;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrap(
            trapOid, m_switchId, 3, attrs));
    vppMock().coppDhcpBindCalls.clear();

    sai_attribute_t setAttr{};
    setAttr.id = SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION;
    setAttr.value.s32 = SAI_PACKET_ACTION_TRAP;
    m_sw->setInternalResult = SAI_STATUS_FAILURE;
    EXPECT_EQ(SAI_STATUS_FAILURE, m_sw->setHostifTrap(
            sai_serialize_object_id(trapOid), &setAttr));

    ASSERT_EQ(2u, vppMock().coppDhcpBindCalls.size());
    EXPECT_FALSE(vppMock().coppDhcpBindCalls[0].isCopy);
    EXPECT_TRUE(vppMock().coppDhcpBindCalls[1].isCopy);
    EXPECT_EQ(SAI_PACKET_ACTION_COPY,
            m_sw->m_trap_binding_map.at(0x10004).packet_action);
    EXPECT_EQ(SAI_PACKET_ACTION_COPY,
            m_sw->m_trap_map.at(trapOid).packet_action);
}

TEST_F(SwitchVppNeighborMissTrap, DhcpGroupRollbackRestoresEffectiveSharedAction)
{
    const sai_object_id_t trapGroup = 0x1200000080;
    const sai_object_id_t copyGroup = 0x1200000081;
    const sai_object_id_t trapPolicer = 0x1200000082;
    const sai_object_id_t copyPolicer = 0x1200000083;
    const sai_object_id_t newPolicer = 0x1200000084;
    const sai_object_id_t trapOid = 0x1200000085;
    const sai_object_id_t copyOid = 0x1200000086;

    sai_attribute_t groupAttr{};
    groupAttr.id = SAI_HOSTIF_TRAP_GROUP_ATTR_POLICER;
    groupAttr.value.oid = trapPolicer;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrapGroup(
            trapGroup, m_switchId, 1, &groupAttr));
    groupAttr.value.oid = copyPolicer;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrapGroup(
            copyGroup, m_switchId, 1, &groupAttr));

    vpp_policer_entry_t policer{};
    policer.vpp_policer_index = 82;
    m_sw->m_policer_map[trapPolicer] = policer;
    policer.vpp_policer_index = 83;
    m_sw->m_policer_map[copyPolicer] = policer;
    policer.vpp_policer_index = 84;
    m_sw->m_policer_map[newPolicer] = policer;

    vpp_trap_entry_t trap{};
    trap.trap_type = SAI_HOSTIF_TRAP_TYPE_DHCP;
    trap.packet_action = SAI_PACKET_ACTION_TRAP;
    trap.trap_group_oid = trapGroup;
    trap.classify_installed = true;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->installTrapClassifyNow(
            trapOid, trap, 82, trapPolicer));
    m_sw->m_trap_map[trapOid] = trap;

    vpp_trap_entry_t copy = trap;
    copy.packet_action = SAI_PACKET_ACTION_COPY;
    copy.trap_group_oid = copyGroup;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->installTrapClassifyNow(
            copyOid, copy, 83, copyPolicer));
    m_sw->m_trap_map[copyOid] = copy;
    vppMock().coppDhcpBindCalls.clear();

    vppMock().coppDhcpBindFailOnCall = 1;
    groupAttr.value.oid = newPolicer;
    EXPECT_EQ(SAI_STATUS_FAILURE, m_sw->setHostifTrapGroup(
            sai_serialize_object_id(copyGroup), &groupAttr));

    ASSERT_EQ(3u, vppMock().coppDhcpBindCalls.size());
    EXPECT_FALSE(vppMock().coppDhcpBindCalls[0].isCopy);
    EXPECT_FALSE(vppMock().coppDhcpBindCalls[1].isBind);
    EXPECT_FALSE(vppMock().coppDhcpBindCalls[2].isCopy);
    EXPECT_EQ("copp-policer-0x1200000083",
            vppMock().coppDhcpBindCalls[2].policerName);
    EXPECT_EQ(copyPolicer,
            m_sw->m_trap_binding_map.at(0x10003).policer_oid);
    EXPECT_EQ(SAI_PACKET_ACTION_TRAP,
            m_sw->m_trap_binding_map.at(0x10003).packet_action);
}

class SwitchVppDhcpVlanPunt : public ::testing::Test
{
    protected:

        void SetUp() override
        {
            vppMock().reset();
            vppMock().classifyAvailable = true;
            vppMock().enabledFeatures.insert("l2-trap-fixup");
            vppMock().enabledFeatures.insert("l2-vlan-filter");

            auto sc = std::make_shared<SwitchConfig>(0, "");
            sc->m_saiSwitchType = SAI_SWITCH_TYPE_NPU;
            sc->m_switchType = SAI_VS_SWITCH_TYPE_VPP;
            sc->m_bootType = SAI_VS_BOOT_TYPE_COLD;
            sc->m_useTapDevice = false;
            sc->m_profileMap[SAI_KEY_VS_PORT_CONFIG_FILE] = "/dev/null";
            sc->m_laneMap = LaneMap::getDefaultLaneMap(0);
            sc->m_eventQueue = std::make_shared<EventQueue>(
                    std::make_shared<Signal>());

            auto scc = std::make_shared<SwitchConfigContainer>();
            scc->insert(sc);
            m_mgr = std::make_shared<RealObjectIdManager>(0, scc);
            m_sw = std::make_shared<TestableSwitchVpp>(m_switchId, m_mgr, sc);
            sc->m_useTapDevice = true;

            m_vlan = m_mgr->allocateNewObjectId(
                    SAI_OBJECT_TYPE_VLAN, m_switchId);
            sai_attribute_t vattr{};
            vattr.id = SAI_VLAN_ATTR_VLAN_ID;
            vattr.value.u16 = VLAN_ID;
            ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->create_internal(
                    SAI_OBJECT_TYPE_VLAN, sai_serialize_object_id(m_vlan),
                    m_switchId, 1, &vattr));
        }

        void TearDown() override
        {
            m_sw->m_switchConfig->m_useTapDevice = false;
            m_sw.reset();
        }

        void addMember(const char *hwif, bool tagged)
        {
            auto port = m_mgr->allocateNewObjectId(
                    SAI_OBJECT_TYPE_PORT, m_switchId);
            sai_attribute_t pattr{};
            pattr.id = SAI_PORT_ATTR_ADMIN_STATE;
            pattr.value.booldata = false;
            ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->create_internal(
                    SAI_OBJECT_TYPE_PORT, sai_serialize_object_id(port),
                    m_switchId, 1, &pattr));
            ASSERT_NE(nullptr, m_sw->m_ifaceRegistry.addPhysicalPort(
                    hwif, std::string("os_") + hwif, port));

            auto bridgePort = m_mgr->allocateNewObjectId(
                    SAI_OBJECT_TYPE_BRIDGE_PORT, m_switchId);
            sai_attribute_t battrs[2]{};
            battrs[0].id = SAI_BRIDGE_PORT_ATTR_TYPE;
            battrs[0].value.s32 = SAI_BRIDGE_PORT_TYPE_PORT;
            battrs[1].id = SAI_BRIDGE_PORT_ATTR_PORT_ID;
            battrs[1].value.oid = port;
            ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->create_internal(
                    SAI_OBJECT_TYPE_BRIDGE_PORT,
                    sai_serialize_object_id(bridgePort), m_switchId,
                    2, battrs));

            sai_attribute_t mattrs[3]{};
            mattrs[0].id = SAI_VLAN_MEMBER_ATTR_VLAN_ID;
            mattrs[0].value.oid = m_vlan;
            mattrs[1].id = SAI_VLAN_MEMBER_ATTR_BRIDGE_PORT_ID;
            mattrs[1].value.oid = bridgePort;
            mattrs[2].id = SAI_VLAN_MEMBER_ATTR_VLAN_TAGGING_MODE;
            mattrs[2].value.s32 = tagged ? SAI_VLAN_TAGGING_MODE_TAGGED :
                SAI_VLAN_TAGGING_MODE_UNTAGGED;
            ASSERT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->vpp_create_vlan_member(3, mattrs));
        }

        static std::vector<uint8_t> dhcp6(
                bool tagged, uint16_t dport = 547, bool extension = false)
        {
            std::vector<uint8_t> frame = {
                0x33, 0x33, 0x00, 0x01, 0x00, 0x02,
                0x02, 0x00, 0x00, 0x00, 0x00, 0x01
            };
            if (tagged)
            {
                frame.insert(frame.end(), {
                    0x81, 0x00, 0x00, (uint8_t)VLAN_ID
                });
            }
            frame.insert(frame.end(), {0x86, 0xdd});

            std::vector<uint8_t> ip6(extension ? 56 : 48, 0);
            ip6[0] = 0x60;
            ip6[4] = 0; ip6[5] = extension ? 16 : 8;
            ip6[6] = extension ? 0 : 17;
            ip6[7] = 64;
            ip6[8 + 15] = 1; // source ::1
            const uint8_t dst[16] = {
                0xff, 0x02, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 1, 0, 2
            };
            memcpy(&ip6[24], dst, sizeof(dst));
            size_t udp = extension ? 48 : 40;
            if (extension)
            {
                ip6[40] = 17; // Hop-by-Hop next header is UDP
                ip6[41] = 0;  // 8-byte extension header
            }
            ip6[udp] = (uint8_t)(546 >> 8);
            ip6[udp + 1] = (uint8_t)(546 & 0xff);
            ip6[udp + 2] = (uint8_t)(dport >> 8);
            ip6[udp + 3] = (uint8_t)(dport & 0xff);
            ip6[udp + 4] = 0; ip6[udp + 5] = 8;
            frame.insert(frame.end(), ip6.begin(), ip6.end());
            return frame;
        }

        static constexpr uint16_t VLAN_ID = 100;
        const sai_object_id_t m_switchId = 0x2100000000;
        sai_object_id_t m_vlan = SAI_NULL_OBJECT_ID;
        std::shared_ptr<RealObjectIdManager> m_mgr;
        std::shared_ptr<TestableSwitchVpp> m_sw;
};

constexpr uint16_t SwitchVppDhcpVlanPunt::VLAN_ID;

TEST_F(SwitchVppDhcpVlanPunt, LeavesDhcpCandidatesForProviderPostAdmissionPath)
{
    addMember("bobm0", false);
    addMember("bobm1", true);

    // DHCP interception is no longer a terminal fixed-offset classifier;
    // the provider handles it only after ingress VLAN admission.
    // sonic-ext's post-admission DHCP feature parses the live packet and
    // applies TRAP/COPY from the semantic binding, so absent traps preserve
    // ordinary VLAN forwarding.
    EXPECT_EQ(~0u, vppMock().classifyFrame(
            "bobm0", 1, dhcp6(false)));
    EXPECT_EQ(~0u, vppMock().classifyFrame(
            "bobm1.100", 2, dhcp6(true)));
    EXPECT_EQ(~0u, vppMock().classifyFrame(
            "bobm0", 1, dhcp6(false, 547, true)));
    EXPECT_EQ(~0u, vppMock().classifyFrame(
            "bobm1.100", 2, dhcp6(true, 547, true)));
}

TEST_F(SwitchVppDhcpVlanPunt, MulticastFloodNoneKeepsDhcpV6InTerminalFloodPolicy)
{
    sai_attribute_t floodAttr{};
    floodAttr.id = SAI_VLAN_ATTR_UNKNOWN_MULTICAST_FLOOD_CONTROL_TYPE;
    floodAttr.value.s32 = SAI_VLAN_FLOOD_CONTROL_TYPE_NONE;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->set_internal(
            SAI_OBJECT_TYPE_VLAN, sai_serialize_object_id(m_vlan),
            &floodAttr));

    addMember("bobm4", false);
    addMember("bobm5", true);

    uint32_t puntNext = vppMock().nodeNext(
            "l2-input-classify", "linux-cp-punt");
    uint32_t fixupNext = vppMock().nodeNext(
            "l2-input-classify", "sonic-ext-l2-trap-fixup");
    ASSERT_NE(~0u, puntNext);
    ASSERT_NE(~0u, fixupNext);

    // COPY preserves normal forwarding eligibility; it does not override an
    // explicit flood-NONE policy. These terminal paths still traverse
    // copp-ifout on the member host tap, where the live DHCPv6 row is metered.
    EXPECT_EQ(puntNext, vppMock().classifyFrame(
            "bobm4", 1, dhcp6(false, 547, true)));
    EXPECT_EQ(fixupNext, vppMock().classifyFrame(
            "bobm5.100", 2, dhcp6(true, 547, true)));
}

TEST_F(SwitchVppDhcpVlanPunt, RebuildsClassifierAfterMockReset)
{
    addMember("bobm2", false);
    uint32_t puntNext = vppMock().nodeNext(
            "l2-input-classify", "linux-cp-punt");
    ASSERT_NE(~0u, puntNext);

    std::vector<uint8_t> lldp(16, 0);
    lldp[12] = 0x88;
    lldp[13] = 0xcc;
    EXPECT_EQ(puntNext, vppMock().classifyFrame("bobm2", 2, lldp));
}

TEST_F(SwitchVppNeighborMissTrap, BindsAndUnbindsTheNeighborMissPolicer)
{
    const sai_object_id_t trapGroupOid = 0x1200000001;
    const sai_object_id_t policerOid = 0x1200000002;
    const sai_object_id_t trapOid = 0x1200000003;

    vpp_trap_group_entry_t group{};
    group.admin_state = true;
    group.policer_oid = policerOid;
    m_sw->m_trap_group_map[trapGroupOid] = group;

    vpp_trap_entry_t trap{};
    trap.trap_type = SAI_HOSTIF_TRAP_TYPE_NEIGHBOR_MISS;
    trap.packet_action = SAI_PACKET_ACTION_TRAP;
    trap.trap_group_oid = trapGroupOid;

    ASSERT_EQ(SAI_STATUS_SUCCESS,
            m_sw->installTrapClassifyNow(trapOid, trap, 7));
    ASSERT_EQ(1u, vppMock().coppNeighborMissBindCalls.size());

    const auto& bind = vppMock().coppNeighborMissBindCalls[0];
    EXPECT_EQ("copp-policer-0x1200000002", bind.policerName);
    EXPECT_TRUE(bind.isBind);

    ASSERT_EQ(SAI_STATUS_SUCCESS,
            m_sw->uninstallTrapClassifyNow(trapOid, trap));
    ASSERT_EQ(2u, vppMock().coppNeighborMissBindCalls.size());

    const auto& unbind = vppMock().coppNeighborMissBindCalls[1];
    EXPECT_TRUE(unbind.policerName.empty());
    EXPECT_FALSE(unbind.isBind);
}

TEST_F(SwitchVppNeighborMissTrap, PropagatesNeighborMissBindFailure)
{
    const sai_object_id_t trapGroupOid = 0x1200000001;

    vpp_trap_group_entry_t group{};
    group.admin_state = true;
    group.policer_oid = 0x1200000002;
    m_sw->m_trap_group_map[trapGroupOid] = group;

    vpp_trap_entry_t trap{};
    trap.trap_type = SAI_HOSTIF_TRAP_TYPE_NEIGHBOR_MISS;
    trap.packet_action = SAI_PACKET_ACTION_TRAP;
    trap.trap_group_oid = trapGroupOid;

    vppMock().coppNeighborMissBindResult = -1;
    EXPECT_EQ(SAI_STATUS_FAILURE,
            m_sw->installTrapClassifyNow(0x1200000003, trap, 7));
}

TEST_F(SwitchVppNeighborMissTrap, ReplacesTheSingletonBindingOnPolicerChange)
{
    const sai_object_id_t trapGroupOid = 0x1200000001;
    const sai_object_id_t trapOid = 0x1200000003;

    vpp_trap_group_entry_t group{};
    group.admin_state = true;
    group.policer_oid = 0x1200000002;
    m_sw->m_trap_group_map[trapGroupOid] = group;

    vpp_trap_entry_t trap{};
    trap.trap_type = SAI_HOSTIF_TRAP_TYPE_NEIGHBOR_MISS;
    trap.packet_action = SAI_PACKET_ACTION_COPY;
    trap.trap_group_oid = trapGroupOid;

    ASSERT_EQ(SAI_STATUS_SUCCESS,
            m_sw->installTrapClassifyNow(trapOid, trap, 7));

    m_sw->m_trap_group_map[trapGroupOid].policer_oid = 0x1200000004;
    ASSERT_EQ(SAI_STATUS_SUCCESS,
            m_sw->installTrapClassifyNow(trapOid, trap, 8));

    ASSERT_EQ(2u, vppMock().coppNeighborMissBindCalls.size());
    EXPECT_EQ("copp-policer-0x1200000002",
            vppMock().coppNeighborMissBindCalls[0].policerName);
    EXPECT_EQ("copp-policer-0x1200000004",
            vppMock().coppNeighborMissBindCalls[1].policerName);
    EXPECT_TRUE(vppMock().coppNeighborMissBindCalls[0].isBind);
    EXPECT_TRUE(vppMock().coppNeighborMissBindCalls[1].isBind);
}

TEST_F(SwitchVppNeighborMissTrap, RejectsUnsupportedNeighborMissDropAction)
{
    const sai_object_id_t trapGroupOid = 0x1200000001;
    const sai_object_id_t policerOid = 0x1200000002;
    const sai_object_id_t trapOid = 0x1200000003;

    sai_attribute_t groupAttr{};
    groupAttr.id = SAI_HOSTIF_TRAP_GROUP_ATTR_POLICER;
    groupAttr.value.oid = policerOid;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrapGroup(
            trapGroupOid, m_switchId, 1, &groupAttr));

    vpp_policer_entry_t policer{};
    policer.vpp_policer_index = 7;
    m_sw->m_policer_map[policerOid] = policer;

    sai_attribute_t trapAttrs[3]{};
    trapAttrs[0].id = SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE;
    trapAttrs[0].value.s32 = SAI_HOSTIF_TRAP_TYPE_NEIGHBOR_MISS;
    trapAttrs[1].id = SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION;
    trapAttrs[1].value.s32 = SAI_PACKET_ACTION_DROP;
    trapAttrs[2].id = SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP;
    trapAttrs[2].value.oid = trapGroupOid;
    EXPECT_EQ(SAI_STATUS_NOT_SUPPORTED, m_sw->createHostifTrap(
            trapOid, m_switchId, 3, trapAttrs));
    EXPECT_EQ(0u, m_sw->m_trap_map.count(trapOid));
    EXPECT_TRUE(vppMock().coppNeighborMissBindCalls.empty());

    trapAttrs[1].value.s32 = SAI_PACKET_ACTION_TRAP;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrap(
            trapOid, m_switchId, 3, trapAttrs));
    sai_attribute_t setAttr{};
    setAttr.id = SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION;
    setAttr.value.s32 = SAI_PACKET_ACTION_DROP;
    EXPECT_EQ(SAI_STATUS_NOT_SUPPORTED, m_sw->setHostifTrap(
            sai_serialize_object_id(trapOid), &setAttr));
    EXPECT_EQ(SAI_PACKET_ACTION_TRAP,
            m_sw->m_trap_map.at(trapOid).packet_action);
}

TEST_F(SwitchVppNeighborMissTrap, NullTrapGroupPolicerKeepsActiveNeighborMissUnpoliced)
{
    const sai_object_id_t trapGroupOid = 0x1200000001;
    const sai_object_id_t policerOid = 0x1200000002;
    const sai_object_id_t trapOid = 0x1200000003;

    sai_attribute_t groupAttr{};
    groupAttr.id = SAI_HOSTIF_TRAP_GROUP_ATTR_POLICER;
    groupAttr.value.oid = policerOid;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrapGroup(
            trapGroupOid, m_switchId, 1, &groupAttr));

    vpp_policer_entry_t policer{};
    policer.vpp_policer_index = 7;
    m_sw->m_policer_map[policerOid] = policer;

    sai_attribute_t trapAttrs[3]{};
    trapAttrs[0].id = SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE;
    trapAttrs[0].value.s32 = SAI_HOSTIF_TRAP_TYPE_NEIGHBOR_MISS;
    trapAttrs[1].id = SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION;
    trapAttrs[1].value.s32 = SAI_PACKET_ACTION_TRAP;
    trapAttrs[2].id = SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP;
    trapAttrs[2].value.oid = trapGroupOid;

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrap(
            trapOid, m_switchId, 3, trapAttrs));
    ASSERT_EQ(1u, vppMock().coppNeighborMissBindCalls.size());
    EXPECT_TRUE(vppMock().coppNeighborMissBindCalls[0].isBind);

    groupAttr.value.oid = SAI_NULL_OBJECT_ID;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->setHostifTrapGroup(
            sai_serialize_object_id(trapGroupOid), &groupAttr));

    ASSERT_EQ(2u, vppMock().coppNeighborMissBindCalls.size());
    EXPECT_TRUE(vppMock().coppNeighborMissBindCalls[1].isBind);
    EXPECT_TRUE(vppMock().coppNeighborMissBindCalls[1].policerName.empty());
    ASSERT_EQ(1u, m_sw->m_trap_binding_map.size());
    EXPECT_EQ(SAI_NULL_OBJECT_ID, m_sw->m_trap_binding_map.begin()->second.policer_oid);
    EXPECT_EQ(SAI_NULL_OBJECT_ID,
            m_sw->m_trap_group_map.at(trapGroupOid).policer_oid);

    sai_attribute_t readAttr{};
    readAttr.id = SAI_HOSTIF_TRAP_GROUP_ATTR_POLICER;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->get(
            SAI_OBJECT_TYPE_HOSTIF_TRAP_GROUP,
            sai_serialize_object_id(trapGroupOid), 1, &readAttr));
    EXPECT_EQ(SAI_NULL_OBJECT_ID, readAttr.value.oid);
}

TEST_F(SwitchVppNeighborMissTrap, CreateBindFailureRollsBackSaiObjectAndAllowsRetry)
{
    const sai_object_id_t trapGroupOid = 0x1200000001;
    const sai_object_id_t policerOid = 0x1200000002;
    const sai_object_id_t trapOid = 0x1200000003;

    sai_attribute_t groupAttr{};
    groupAttr.id = SAI_HOSTIF_TRAP_GROUP_ATTR_POLICER;
    groupAttr.value.oid = policerOid;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrapGroup(
            trapGroupOid, m_switchId, 1, &groupAttr));

    vpp_policer_entry_t policer{};
    policer.vpp_policer_index = 7;
    m_sw->m_policer_map[policerOid] = policer;

    sai_attribute_t trapAttrs[3]{};
    trapAttrs[0].id = SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE;
    trapAttrs[0].value.s32 = SAI_HOSTIF_TRAP_TYPE_NEIGHBOR_MISS;
    trapAttrs[1].id = SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION;
    trapAttrs[1].value.s32 = SAI_PACKET_ACTION_TRAP;
    trapAttrs[2].id = SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP;
    trapAttrs[2].value.oid = trapGroupOid;

    vppMock().coppNeighborMissBindResult = -1;
    EXPECT_EQ(SAI_STATUS_FAILURE, m_sw->createHostifTrap(
            trapOid, m_switchId, 3, trapAttrs));
    EXPECT_EQ(0u, m_sw->m_trap_map.count(trapOid));

    sai_attribute_t readAttr{};
    readAttr.id = SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE;
    EXPECT_EQ(SAI_STATUS_ITEM_NOT_FOUND, m_sw->get(
            SAI_OBJECT_TYPE_HOSTIF_TRAP, sai_serialize_object_id(trapOid),
            1, &readAttr));

    vppMock().coppNeighborMissBindResult = 0;
    EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrap(
            trapOid, m_switchId, 3, trapAttrs));
    EXPECT_EQ(1u, m_sw->m_trap_map.count(trapOid));
    EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->get(
            SAI_OBJECT_TYPE_HOSTIF_TRAP, sai_serialize_object_id(trapOid),
            1, &readAttr));
}

TEST_F(SwitchVppNeighborMissTrap, TrapGroupBindFailureKeepsOldCachedState)
{
    const sai_object_id_t trapGroupOid = 0x1200000001;
    const sai_object_id_t oldPolicerOid = 0x1200000002;
    const sai_object_id_t trapOid = 0x1200000003;
    const sai_object_id_t newPolicerOid = 0x1200000004;

    sai_attribute_t groupAttr{};
    groupAttr.id = SAI_HOSTIF_TRAP_GROUP_ATTR_POLICER;
    groupAttr.value.oid = oldPolicerOid;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrapGroup(
            trapGroupOid, m_switchId, 1, &groupAttr));

    vpp_policer_entry_t oldPolicer{};
    oldPolicer.vpp_policer_index = 7;
    m_sw->m_policer_map[oldPolicerOid] = oldPolicer;
    vpp_policer_entry_t newPolicer{};
    newPolicer.vpp_policer_index = 8;
    m_sw->m_policer_map[newPolicerOid] = newPolicer;

    sai_attribute_t trapAttrs[3]{};
    trapAttrs[0].id = SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE;
    trapAttrs[0].value.s32 = SAI_HOSTIF_TRAP_TYPE_NEIGHBOR_MISS;
    trapAttrs[1].id = SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION;
    trapAttrs[1].value.s32 = SAI_PACKET_ACTION_TRAP;
    trapAttrs[2].id = SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP;
    trapAttrs[2].value.oid = trapGroupOid;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrap(
            trapOid, m_switchId, 3, trapAttrs));

    vppMock().coppNeighborMissBindCalls.clear();
    vppMock().coppNeighborMissBindFailOnCall = 1;
    groupAttr.value.oid = newPolicerOid;
    EXPECT_EQ(SAI_STATUS_FAILURE, m_sw->setHostifTrapGroup(
            sai_serialize_object_id(trapGroupOid), &groupAttr));

    EXPECT_EQ(oldPolicerOid,
            m_sw->m_trap_group_map.at(trapGroupOid).policer_oid);
    sai_attribute_t readAttr{};
    readAttr.id = SAI_HOSTIF_TRAP_GROUP_ATTR_POLICER;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->get(
            SAI_OBJECT_TYPE_HOSTIF_TRAP_GROUP,
            sai_serialize_object_id(trapGroupOid), 1, &readAttr));
    EXPECT_EQ(oldPolicerOid, readAttr.value.oid);
}

TEST_F(SwitchVppNeighborMissTrap, TrapGroupNullUnbindFailureKeepsOldCachedState)
{
    const sai_object_id_t trapGroupOid = 0x1200000001;
    const sai_object_id_t policerOid = 0x1200000002;
    const sai_object_id_t trapOid = 0x1200000003;

    sai_attribute_t groupAttr{};
    groupAttr.id = SAI_HOSTIF_TRAP_GROUP_ATTR_POLICER;
    groupAttr.value.oid = policerOid;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrapGroup(
            trapGroupOid, m_switchId, 1, &groupAttr));

    vpp_policer_entry_t policer{};
    policer.vpp_policer_index = 7;
    m_sw->m_policer_map[policerOid] = policer;

    sai_attribute_t trapAttrs[3]{};
    trapAttrs[0].id = SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE;
    trapAttrs[0].value.s32 = SAI_HOSTIF_TRAP_TYPE_NEIGHBOR_MISS;
    trapAttrs[1].id = SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION;
    trapAttrs[1].value.s32 = SAI_PACKET_ACTION_TRAP;
    trapAttrs[2].id = SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP;
    trapAttrs[2].value.oid = trapGroupOid;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrap(
            trapOid, m_switchId, 3, trapAttrs));

    vppMock().coppNeighborMissBindCalls.clear();
    vppMock().coppNeighborMissBindFailOnCall = 1;
    groupAttr.value.oid = SAI_NULL_OBJECT_ID;
    EXPECT_EQ(SAI_STATUS_FAILURE, m_sw->setHostifTrapGroup(
            sai_serialize_object_id(trapGroupOid), &groupAttr));

    ASSERT_EQ(2u, vppMock().coppNeighborMissBindCalls.size());
    EXPECT_TRUE(vppMock().coppNeighborMissBindCalls[0].isBind);
    EXPECT_TRUE(vppMock().coppNeighborMissBindCalls[0].policerName.empty());
    EXPECT_TRUE(vppMock().coppNeighborMissBindCalls[1].isBind);
    EXPECT_EQ("copp-policer-0x1200000002",
            vppMock().coppNeighborMissBindCalls[1].policerName);
    EXPECT_EQ(policerOid,
            m_sw->m_trap_group_map.at(trapGroupOid).policer_oid);
    sai_attribute_t readAttr{};
    readAttr.id = SAI_HOSTIF_TRAP_GROUP_ATTR_POLICER;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->get(
            SAI_OBJECT_TYPE_HOSTIF_TRAP_GROUP,
            sai_serialize_object_id(trapGroupOid), 1, &readAttr));
    EXPECT_EQ(policerOid, readAttr.value.oid);
}

TEST_F(SwitchVppNeighborMissTrap, MixedTrapGroupFailureRestoresAbsentLldpBinding)
{
    const sai_object_id_t trapGroupOid = 0x1200000001;
    const sai_object_id_t policerOid = 0x1200000002;
    const sai_object_id_t lldpTrapOid = 0x1200000003;
    const sai_object_id_t neighborTrapOid = 0x1200000004;

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrapGroup(
            trapGroupOid, m_switchId, 0, nullptr));

    vpp_policer_entry_t policer{};
    policer.vpp_policer_index = 7;
    m_sw->m_policer_map[policerOid] = policer;

    auto createTrap = [&](sai_object_id_t trapOid,
            sai_hostif_trap_type_t trapType)
    {
        sai_attribute_t attrs[3]{};
        attrs[0].id = SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE;
        attrs[0].value.s32 = trapType;
        attrs[1].id = SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION;
        attrs[1].value.s32 = SAI_PACKET_ACTION_TRAP;
        attrs[2].id = SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP;
        attrs[2].value.oid = trapGroupOid;
        return m_sw->createHostifTrap(trapOid, m_switchId, 3, attrs);
    };

    ASSERT_EQ(SAI_STATUS_SUCCESS,
            createTrap(lldpTrapOid, SAI_HOSTIF_TRAP_TYPE_LLDP));
    ASSERT_EQ(SAI_STATUS_SUCCESS,
            createTrap(neighborTrapOid, SAI_HOSTIF_TRAP_TYPE_NEIGHBOR_MISS));

    vppMock().coppIfoutBindCalls.clear();
    vppMock().coppNeighborMissBindCalls.clear();
    vppMock().coppNeighborMissBindFailOnCall = 1;

    sai_attribute_t groupAttr{};
    groupAttr.id = SAI_HOSTIF_TRAP_GROUP_ATTR_POLICER;
    groupAttr.value.oid = policerOid;
    EXPECT_EQ(SAI_STATUS_FAILURE, m_sw->setHostifTrapGroup(
            sai_serialize_object_id(trapGroupOid), &groupAttr));

    ASSERT_EQ(2u, vppMock().coppIfoutBindCalls.size());
    EXPECT_TRUE(vppMock().coppIfoutBindCalls[0].isBind);
    EXPECT_EQ("copp-policer-0x1200000002",
            vppMock().coppIfoutBindCalls[0].policerName);
    EXPECT_FALSE(vppMock().coppIfoutBindCalls[1].isBind);

    ASSERT_EQ(2u, vppMock().coppNeighborMissBindCalls.size());
    EXPECT_TRUE(vppMock().coppNeighborMissBindCalls[0].isBind);
    EXPECT_TRUE(vppMock().coppNeighborMissBindCalls[1].isBind);
    EXPECT_TRUE(vppMock().coppNeighborMissBindCalls[1].policerName.empty());
    EXPECT_EQ(SAI_NULL_OBJECT_ID,
            m_sw->m_trap_group_map.at(trapGroupOid).policer_oid);
}

TEST_F(SwitchVppNeighborMissTrap, MixedTrapRollbackRestoresSharedLldpOwner)
{
    const sai_object_id_t updatingGroupOid = 0x1200000001;
    const sai_object_id_t updatingPolicerOid = 0x1200000002;
    const sai_object_id_t updatingLldpTrapOid = 0x1200000003;
    const sai_object_id_t neighborTrapOid = 0x1200000004;
    const sai_object_id_t fallbackGroupOid = 0x1200000005;
    const sai_object_id_t fallbackPolicerOid = 0x1200000006;
    const sai_object_id_t fallbackLldpTrapOid = 0x1200000007;

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrapGroup(
            updatingGroupOid, m_switchId, 0, nullptr));

    sai_attribute_t groupAttr{};
    groupAttr.id = SAI_HOSTIF_TRAP_GROUP_ATTR_POLICER;
    groupAttr.value.oid = fallbackPolicerOid;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrapGroup(
            fallbackGroupOid, m_switchId, 1, &groupAttr));

    vpp_policer_entry_t updatingPolicer{};
    updatingPolicer.vpp_policer_index = 7;
    m_sw->m_policer_map[updatingPolicerOid] = updatingPolicer;
    vpp_policer_entry_t fallbackPolicer{};
    fallbackPolicer.vpp_policer_index = 8;
    m_sw->m_policer_map[fallbackPolicerOid] = fallbackPolicer;

    auto createTrap = [&](sai_object_id_t trapOid,
            sai_hostif_trap_type_t trapType, sai_object_id_t groupOid)
    {
        sai_attribute_t attrs[3]{};
        attrs[0].id = SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE;
        attrs[0].value.s32 = trapType;
        attrs[1].id = SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION;
        attrs[1].value.s32 = SAI_PACKET_ACTION_TRAP;
        attrs[2].id = SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP;
        attrs[2].value.oid = groupOid;
        return m_sw->createHostifTrap(trapOid, m_switchId, 3, attrs);
    };

    ASSERT_EQ(SAI_STATUS_SUCCESS, createTrap(
            fallbackLldpTrapOid, SAI_HOSTIF_TRAP_TYPE_LLDP,
            fallbackGroupOid));
    ASSERT_EQ(SAI_STATUS_SUCCESS, createTrap(
            updatingLldpTrapOid, SAI_HOSTIF_TRAP_TYPE_LLDP,
            updatingGroupOid));
    ASSERT_EQ(SAI_STATUS_SUCCESS, createTrap(
            neighborTrapOid, SAI_HOSTIF_TRAP_TYPE_NEIGHBOR_MISS,
            updatingGroupOid));

    vppMock().coppIfoutBindCalls.clear();
    vppMock().coppNeighborMissBindCalls.clear();
    vppMock().coppNeighborMissBindFailOnCall = 1;
    groupAttr.value.oid = updatingPolicerOid;
    EXPECT_EQ(SAI_STATUS_FAILURE, m_sw->setHostifTrapGroup(
            sai_serialize_object_id(updatingGroupOid), &groupAttr));

    ASSERT_EQ(2u, vppMock().coppIfoutBindCalls.size());
    EXPECT_EQ("copp-policer-0x1200000002",
            vppMock().coppIfoutBindCalls[0].policerName);
    EXPECT_EQ("copp-policer-0x1200000006",
            vppMock().coppIfoutBindCalls[1].policerName);
    EXPECT_TRUE(vppMock().coppIfoutBindCalls[1].isBind);
    EXPECT_EQ(SAI_NULL_OBJECT_ID,
            m_sw->m_trap_group_map.at(updatingGroupOid).policer_oid);
}

TEST_F(SwitchVppNeighborMissTrap, FailedSharedArpRebindRestoresEffectiveOwner)
{
    const sai_object_id_t requestGroupOid = 0x1200000001;
    const sai_object_id_t requestPolicerOid = 0x1200000002;
    const sai_object_id_t requestTrapOid = 0x1200000003;
    const sai_object_id_t responseGroupOid = 0x1200000004;
    const sai_object_id_t responsePolicerOid = 0x1200000005;
    const sai_object_id_t responseTrapOid = 0x1200000006;
    const sai_object_id_t newGroupOid = 0x1200000007;
    const sai_object_id_t newPolicerOid = 0x1200000008;

    auto createGroup = [&](sai_object_id_t groupOid,
            sai_object_id_t policerOid, uint32_t index)
    {
        sai_attribute_t attr{};
        attr.id = SAI_HOSTIF_TRAP_GROUP_ATTR_POLICER;
        attr.value.oid = policerOid;
        EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrapGroup(
                groupOid, m_switchId, 1, &attr));
        vpp_policer_entry_t policer{};
        policer.vpp_policer_index = index;
        m_sw->m_policer_map[policerOid] = policer;
    };
    createGroup(requestGroupOid, requestPolicerOid, 7);
    createGroup(responseGroupOid, responsePolicerOid, 8);
    createGroup(newGroupOid, newPolicerOid, 9);

    auto createArpTrap = [&](sai_object_id_t trapOid,
            sai_hostif_trap_type_t trapType, sai_object_id_t groupOid)
    {
        sai_attribute_t attrs[3]{};
        attrs[0].id = SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE;
        attrs[0].value.s32 = trapType;
        attrs[1].id = SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION;
        attrs[1].value.s32 = SAI_PACKET_ACTION_TRAP;
        attrs[2].id = SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP;
        attrs[2].value.oid = groupOid;
        return m_sw->createHostifTrap(trapOid, m_switchId, 3, attrs);
    };
    ASSERT_EQ(SAI_STATUS_SUCCESS, createArpTrap(requestTrapOid,
            SAI_HOSTIF_TRAP_TYPE_ARP_REQUEST, requestGroupOid));
    ASSERT_EQ(SAI_STATUS_SUCCESS, createArpTrap(responseTrapOid,
            SAI_HOSTIF_TRAP_TYPE_ARP_RESPONSE, responseGroupOid));

    vppMock().coppIfoutBindCalls.clear();
    vppMock().coppIfoutBindFailOnCall = 1;
    sai_attribute_t setAttr{};
    setAttr.id = SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP;
    setAttr.value.oid = newGroupOid;
    EXPECT_EQ(SAI_STATUS_FAILURE, m_sw->setHostifTrap(
            sai_serialize_object_id(requestTrapOid), &setAttr));

    ASSERT_EQ(2u, vppMock().coppIfoutBindCalls.size());
    EXPECT_EQ("copp-policer-0x1200000008",
            vppMock().coppIfoutBindCalls[0].policerName);
    EXPECT_EQ("copp-policer-0x1200000005",
            vppMock().coppIfoutBindCalls[1].policerName);
    EXPECT_EQ(requestGroupOid,
            m_sw->m_trap_map.at(requestTrapOid).trap_group_oid);
    sai_attribute_t readAttr{};
    readAttr.id = SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->get(
            SAI_OBJECT_TYPE_HOSTIF_TRAP,
            sai_serialize_object_id(requestTrapOid), 1, &readAttr));
    EXPECT_EQ(requestGroupOid, readAttr.value.oid);
    ASSERT_EQ(1u, m_sw->m_trap_binding_map.size());
    EXPECT_EQ(responsePolicerOid,
            m_sw->m_trap_binding_map.begin()->second.policer_oid);
}

TEST_F(SwitchVppNeighborMissTrap, MixedBgpFailureRemovesNewNameBinding)
{
    const sai_object_id_t groupOid = 0x1200000001;
    const sai_object_id_t policerOid = 0x1200000002;
    const sai_object_id_t bgpTrapOid = 0x1200000003;
    const sai_object_id_t neighborTrapOid = 0x1200000004;

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrapGroup(
            groupOid, m_switchId, 0, nullptr));
    vpp_policer_entry_t policer{};
    policer.vpp_policer_index = 7;
    m_sw->m_policer_map[policerOid] = policer;

    auto createTrap = [&](sai_object_id_t trapOid,
            sai_hostif_trap_type_t trapType)
    {
        sai_attribute_t attrs[3]{};
        attrs[0].id = SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE;
        attrs[0].value.s32 = trapType;
        attrs[1].id = SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION;
        attrs[1].value.s32 = SAI_PACKET_ACTION_TRAP;
        attrs[2].id = SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP;
        attrs[2].value.oid = groupOid;
        return m_sw->createHostifTrap(trapOid, m_switchId, 3, attrs);
    };
    ASSERT_EQ(SAI_STATUS_SUCCESS,
            createTrap(bgpTrapOid, SAI_HOSTIF_TRAP_TYPE_BGP));
    ASSERT_EQ(SAI_STATUS_SUCCESS, createTrap(
            neighborTrapOid, SAI_HOSTIF_TRAP_TYPE_NEIGHBOR_MISS));

    vppMock().coppIp2meBindCalls.clear();
    vppMock().coppNeighborMissBindCalls.clear();
    vppMock().coppNeighborMissBindFailOnCall = 1;
    sai_attribute_t attr{};
    attr.id = SAI_HOSTIF_TRAP_GROUP_ATTR_POLICER;
    attr.value.oid = policerOid;
    EXPECT_EQ(SAI_STATUS_FAILURE, m_sw->setHostifTrapGroup(
            sai_serialize_object_id(groupOid), &attr));

    ASSERT_EQ(2u, vppMock().coppIp2meBindCalls.size());
    EXPECT_TRUE(vppMock().coppIp2meBindCalls[0].conditional);
    EXPECT_TRUE(vppMock().coppIp2meBindCalls[0].isBind);
    EXPECT_FALSE(vppMock().coppIp2meBindCalls[1].isBind);
    EXPECT_EQ("copp-policer-0x1200000002",
            vppMock().coppIp2meBindCalls[1].policerName);
    ASSERT_EQ(1u, m_sw->m_trap_binding_map.size());
    EXPECT_EQ(SAI_NULL_OBJECT_ID, m_sw->m_trap_binding_map.begin()->second.policer_oid);
    EXPECT_EQ(SAI_NULL_OBJECT_ID,
            m_sw->m_trap_group_map.at(groupOid).policer_oid);
}

TEST_F(SwitchVppNeighborMissTrap, MixedIp2meFailureRemovesNewNameBinding)
{
    const sai_object_id_t groupOid = 0x1200000001;
    const sai_object_id_t policerOid = 0x1200000002;
    const sai_object_id_t ip2meTrapOid = 0x1200000003;
    const sai_object_id_t neighborTrapOid = 0x1200000004;

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrapGroup(
            groupOid, m_switchId, 0, nullptr));
    vpp_policer_entry_t policer{};
    policer.vpp_policer_index = 7;
    m_sw->m_policer_map[policerOid] = policer;

    auto createTrap = [&](sai_object_id_t trapOid,
            sai_hostif_trap_type_t trapType)
    {
        sai_attribute_t attrs[3]{};
        attrs[0].id = SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE;
        attrs[0].value.s32 = trapType;
        attrs[1].id = SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION;
        attrs[1].value.s32 = SAI_PACKET_ACTION_TRAP;
        attrs[2].id = SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP;
        attrs[2].value.oid = groupOid;
        return m_sw->createHostifTrap(trapOid, m_switchId, 3, attrs);
    };
    ASSERT_EQ(SAI_STATUS_SUCCESS,
            createTrap(ip2meTrapOid, SAI_HOSTIF_TRAP_TYPE_IP2ME));
    ASSERT_EQ(SAI_STATUS_SUCCESS, createTrap(
            neighborTrapOid, SAI_HOSTIF_TRAP_TYPE_NEIGHBOR_MISS));

    vppMock().coppIp2meBindCalls.clear();
    vppMock().coppNeighborMissBindCalls.clear();
    vppMock().coppNeighborMissBindFailOnCall = 1;
    sai_attribute_t attr{};
    attr.id = SAI_HOSTIF_TRAP_GROUP_ATTR_POLICER;
    attr.value.oid = policerOid;
    EXPECT_EQ(SAI_STATUS_FAILURE, m_sw->setHostifTrapGroup(
            sai_serialize_object_id(groupOid), &attr));

    ASSERT_EQ(2u, vppMock().coppIp2meBindCalls.size());
    EXPECT_FALSE(vppMock().coppIp2meBindCalls[0].conditional);
    EXPECT_TRUE(vppMock().coppIp2meBindCalls[0].isBind);
    EXPECT_FALSE(vppMock().coppIp2meBindCalls[1].isBind);
    EXPECT_EQ("copp-policer-0x1200000002",
            vppMock().coppIp2meBindCalls[1].policerName);
    ASSERT_EQ(1u, m_sw->m_trap_binding_map.size());
    EXPECT_EQ(SAI_NULL_OBJECT_ID, m_sw->m_trap_binding_map.begin()->second.policer_oid);
    EXPECT_EQ(SAI_NULL_OBJECT_ID,
            m_sw->m_trap_group_map.at(groupOid).policer_oid);
}

TEST_F(SwitchVppNeighborMissTrap, FailedBgpBindReconcilesAttemptedName)
{
    const sai_object_id_t groupOid = 0x1200000001;
    const sai_object_id_t policerOid = 0x1200000002;
    const sai_object_id_t trapOid = 0x1200000003;

    sai_attribute_t groupAttr{};
    groupAttr.id = SAI_HOSTIF_TRAP_GROUP_ATTR_POLICER;
    groupAttr.value.oid = policerOid;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrapGroup(
            groupOid, m_switchId, 1, &groupAttr));
    vpp_policer_entry_t policer{};
    policer.vpp_policer_index = 7;
    m_sw->m_policer_map[policerOid] = policer;

    sai_attribute_t attrs[3]{};
    attrs[0].id = SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE;
    attrs[0].value.s32 = SAI_HOSTIF_TRAP_TYPE_BGP;
    attrs[1].id = SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION;
    attrs[1].value.s32 = SAI_PACKET_ACTION_TRAP;
    attrs[2].id = SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP;
    attrs[2].value.oid = groupOid;

    vppMock().coppIp2meBindFailOnCall = 1;
    EXPECT_EQ(SAI_STATUS_FAILURE, m_sw->createHostifTrap(
            trapOid, m_switchId, 3, attrs));
    ASSERT_EQ(2u, vppMock().coppIp2meBindCalls.size());
    EXPECT_TRUE(vppMock().coppIp2meBindCalls[0].isBind);
    EXPECT_FALSE(vppMock().coppIp2meBindCalls[1].isBind);
    EXPECT_EQ(vppMock().coppIp2meBindCalls[0].policerName,
            vppMock().coppIp2meBindCalls[1].policerName);
    EXPECT_EQ(0u, m_sw->m_trap_map.count(trapOid));
}

TEST_F(SwitchVppNeighborMissTrap, CleanupFailureStillRestoresPreviousBgpOwner)
{
    const sai_object_id_t groupOid = 0x1200000001;
    const sai_object_id_t oldPolicerOid = 0x1200000002;
    const sai_object_id_t trapOid = 0x1200000003;
    const sai_object_id_t newPolicerOid = 0x1200000004;

    sai_attribute_t groupAttr{};
    groupAttr.id = SAI_HOSTIF_TRAP_GROUP_ATTR_POLICER;
    groupAttr.value.oid = oldPolicerOid;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrapGroup(
            groupOid, m_switchId, 1, &groupAttr));
    vpp_policer_entry_t oldPolicer{};
    oldPolicer.vpp_policer_index = 7;
    m_sw->m_policer_map[oldPolicerOid] = oldPolicer;
    vpp_policer_entry_t newPolicer{};
    newPolicer.vpp_policer_index = 8;
    m_sw->m_policer_map[newPolicerOid] = newPolicer;

    sai_attribute_t attrs[3]{};
    attrs[0].id = SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE;
    attrs[0].value.s32 = SAI_HOSTIF_TRAP_TYPE_BGP;
    attrs[1].id = SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION;
    attrs[1].value.s32 = SAI_PACKET_ACTION_TRAP;
    attrs[2].id = SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP;
    attrs[2].value.oid = groupOid;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrap(
            trapOid, m_switchId, 3, attrs));

    vppMock().coppIp2meBindCalls.clear();
    vppMock().coppIp2meBindFailOnCalls = {1, 2};
    groupAttr.value.oid = newPolicerOid;
    EXPECT_EQ(SAI_STATUS_FAILURE, m_sw->setHostifTrapGroup(
            sai_serialize_object_id(groupOid), &groupAttr));

    ASSERT_EQ(3u, vppMock().coppIp2meBindCalls.size());
    EXPECT_TRUE(vppMock().coppIp2meBindCalls[0].isBind);
    EXPECT_FALSE(vppMock().coppIp2meBindCalls[1].isBind);
    EXPECT_TRUE(vppMock().coppIp2meBindCalls[2].isBind);
    EXPECT_EQ("copp-policer-0x1200000002",
            vppMock().coppIp2meBindCalls[2].policerName);
    EXPECT_EQ(oldPolicerOid,
            m_sw->m_trap_group_map.at(groupOid).policer_oid);
}

TEST_F(SwitchVppNeighborMissTrap, CleanupFailureRetriesOriginallyAbsentBgpOwner)
{
    const sai_object_id_t groupOid = 0x1200000001;
    const sai_object_id_t policerOid = 0x1200000002;
    const sai_object_id_t trapOid = 0x1200000003;

    sai_attribute_t groupAttr{};
    groupAttr.id = SAI_HOSTIF_TRAP_GROUP_ATTR_POLICER;
    groupAttr.value.oid = policerOid;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrapGroup(
            groupOid, m_switchId, 1, &groupAttr));
    vpp_policer_entry_t policer{};
    policer.vpp_policer_index = 7;
    m_sw->m_policer_map[policerOid] = policer;

    sai_attribute_t attrs[3]{};
    attrs[0].id = SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE;
    attrs[0].value.s32 = SAI_HOSTIF_TRAP_TYPE_BGP;
    attrs[1].id = SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION;
    attrs[1].value.s32 = SAI_PACKET_ACTION_TRAP;
    attrs[2].id = SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP;
    attrs[2].value.oid = groupOid;

    // The bind and first cleanup both time out. The originally empty
    // effective-state journal must not suppress a final owner-qualified
    // unbind during restoration.
    vppMock().coppIp2meBindFailOnCalls = {1, 2};
    EXPECT_EQ(SAI_STATUS_FAILURE, m_sw->createHostifTrap(
            trapOid, m_switchId, 3, attrs));

    ASSERT_EQ(3u, vppMock().coppIp2meBindCalls.size());
    EXPECT_TRUE(vppMock().coppIp2meBindCalls[0].isBind);
    EXPECT_FALSE(vppMock().coppIp2meBindCalls[1].isBind);
    EXPECT_FALSE(vppMock().coppIp2meBindCalls[2].isBind);
    EXPECT_EQ(vppMock().coppIp2meBindCalls[0].policerName,
            vppMock().coppIp2meBindCalls[2].policerName);
    EXPECT_TRUE(m_sw->m_trap_binding_map.empty());
    EXPECT_EQ(0u, m_sw->m_trap_map.count(trapOid));
}

TEST_F(SwitchVppNeighborMissTrap, OmittedDefaultNullGroupInstallsUnpolicedRow)
{
    const sai_object_id_t groupOid = 0x1200000001;
    const sai_object_id_t trapOid = 0x1200000002;

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrapGroup(
            groupOid, m_switchId, 0, nullptr));
    sai_attribute_t defaultGroup{};
    defaultGroup.id = SAI_SWITCH_ATTR_DEFAULT_TRAP_GROUP;
    defaultGroup.value.oid = groupOid;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->set_internal(
            SAI_OBJECT_TYPE_SWITCH, sai_serialize_object_id(m_switchId),
            &defaultGroup));

    sai_attribute_t attrs[2]{};
    attrs[0].id = SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE;
    attrs[0].value.s32 = SAI_HOSTIF_TRAP_TYPE_NEIGHBOR_MISS;
    attrs[1].id = SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION;
    attrs[1].value.s32 = SAI_PACKET_ACTION_COPY;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrap(
            trapOid, m_switchId, 2, attrs));

    ASSERT_EQ(1u, vppMock().coppNeighborMissBindCalls.size());
    EXPECT_TRUE(vppMock().coppNeighborMissBindCalls[0].isBind);
    EXPECT_TRUE(vppMock().coppNeighborMissBindCalls[0].policerName.empty());
    ASSERT_EQ(1u, m_sw->m_trap_binding_map.size());
    EXPECT_EQ(SAI_NULL_OBJECT_ID, m_sw->m_trap_binding_map.begin()->second.policer_oid);
}

TEST_F(SwitchVppNeighborMissTrap, UsesDefaultGroupWhenTrapGroupIsOmitted)
{
    const sai_object_id_t groupOid = 0x1200000001;
    const sai_object_id_t policerOid = 0x1200000002;
    const sai_object_id_t trapOid = 0x1200000003;

    sai_attribute_t groupAttr{};
    groupAttr.id = SAI_HOSTIF_TRAP_GROUP_ATTR_POLICER;
    groupAttr.value.oid = policerOid;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrapGroup(
            groupOid, m_switchId, 1, &groupAttr));
    vpp_policer_entry_t policer{};
    policer.vpp_policer_index = 7;
    m_sw->m_policer_map[policerOid] = policer;

    sai_attribute_t defaultGroup{};
    defaultGroup.id = SAI_SWITCH_ATTR_DEFAULT_TRAP_GROUP;
    defaultGroup.value.oid = groupOid;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->set_internal(
            SAI_OBJECT_TYPE_SWITCH, sai_serialize_object_id(m_switchId),
            &defaultGroup));

    sai_attribute_t attrs[2]{};
    attrs[0].id = SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE;
    attrs[0].value.s32 = SAI_HOSTIF_TRAP_TYPE_NEIGHBOR_MISS;
    attrs[1].id = SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION;
    attrs[1].value.s32 = SAI_PACKET_ACTION_TRAP;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrap(
            trapOid, m_switchId, 2, attrs));

    EXPECT_EQ(groupOid, m_sw->m_trap_map.at(trapOid).trap_group_oid);
    ASSERT_EQ(1u, vppMock().coppNeighborMissBindCalls.size());
    EXPECT_EQ("copp-policer-0x1200000002",
            vppMock().coppNeighborMissBindCalls[0].policerName);
    sai_attribute_t readAttr{};
    readAttr.id = SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->get(
            SAI_OBJECT_TYPE_HOSTIF_TRAP,
            sai_serialize_object_id(trapOid), 1, &readAttr));
    EXPECT_EQ(groupOid, readAttr.value.oid);
}

TEST_F(SwitchVppNeighborMissTrap, TtlInstallFailureRestoresOldPolicer)
{
    const sai_object_id_t trapGroupOid = 0x1200000001;
    const sai_object_id_t oldPolicerOid = 0x1200000002;
    const sai_object_id_t trapOid = 0x1200000003;
    const sai_object_id_t newPolicerOid = 0x1200000004;

    sai_attribute_t groupAttr{};
    groupAttr.id = SAI_HOSTIF_TRAP_GROUP_ATTR_POLICER;
    groupAttr.value.oid = oldPolicerOid;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrapGroup(
            trapGroupOid, m_switchId, 1, &groupAttr));

    vpp_policer_entry_t oldPolicer{};
    oldPolicer.vpp_policer_index = 7;
    m_sw->m_policer_map[oldPolicerOid] = oldPolicer;
    vpp_policer_entry_t newPolicer{};
    newPolicer.vpp_policer_index = 8;
    m_sw->m_policer_map[newPolicerOid] = newPolicer;

    sai_attribute_t trapAttrs[3]{};
    trapAttrs[0].id = SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE;
    trapAttrs[0].value.s32 = SAI_HOSTIF_TRAP_TYPE_TTL_ERROR;
    trapAttrs[1].id = SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION;
    trapAttrs[1].value.s32 = SAI_PACKET_ACTION_TRAP;
    trapAttrs[2].id = SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP;
    trapAttrs[2].value.oid = trapGroupOid;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrap(
            trapOid, m_switchId, 3, trapAttrs));

    vppMock().coppIfoutBindCalls.clear();
    vppMock().coppTtlPuntBindCalls.clear();
    vppMock().coppTtlPuntBindFailOnCall = 1;
    groupAttr.value.oid = newPolicerOid;
    EXPECT_EQ(SAI_STATUS_FAILURE, m_sw->setHostifTrapGroup(
            sai_serialize_object_id(trapGroupOid), &groupAttr));

    ASSERT_EQ(2u, vppMock().coppIfoutBindCalls.size());
    EXPECT_EQ("copp-policer-0x1200000004",
            vppMock().coppIfoutBindCalls[0].policerName);
    EXPECT_EQ("copp-policer-0x1200000002",
            vppMock().coppIfoutBindCalls[1].policerName);
    ASSERT_EQ(2u, vppMock().coppTtlPuntBindCalls.size());
    EXPECT_TRUE(vppMock().coppTtlPuntBindCalls[0]);
    EXPECT_TRUE(vppMock().coppTtlPuntBindCalls[1]);
    EXPECT_EQ(oldPolicerOid,
            m_sw->m_trap_group_map.at(trapGroupOid).policer_oid);
}

TEST_F(SwitchVppNeighborMissTrap, TtlFailedRestoreIsRetriedByGroupRollback)
{
    const sai_object_id_t trapGroupOid = 0x1200000001;
    const sai_object_id_t oldPolicerOid = 0x1200000002;
    const sai_object_id_t trapOid = 0x1200000003;
    const sai_object_id_t newPolicerOid = 0x1200000004;

    sai_attribute_t groupAttr{};
    groupAttr.id = SAI_HOSTIF_TRAP_GROUP_ATTR_POLICER;
    groupAttr.value.oid = oldPolicerOid;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrapGroup(
            trapGroupOid, m_switchId, 1, &groupAttr));
    vpp_policer_entry_t oldPolicer{};
    oldPolicer.vpp_policer_index = 7;
    m_sw->m_policer_map[oldPolicerOid] = oldPolicer;
    vpp_policer_entry_t newPolicer{};
    newPolicer.vpp_policer_index = 8;
    m_sw->m_policer_map[newPolicerOid] = newPolicer;

    sai_attribute_t trapAttrs[3]{};
    trapAttrs[0].id = SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE;
    trapAttrs[0].value.s32 = SAI_HOSTIF_TRAP_TYPE_TTL_ERROR;
    trapAttrs[1].id = SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION;
    trapAttrs[1].value.s32 = SAI_PACKET_ACTION_TRAP;
    trapAttrs[2].id = SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP;
    trapAttrs[2].value.oid = trapGroupOid;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrap(
            trapOid, m_switchId, 3, trapAttrs));

    vppMock().coppIfoutBindCalls.clear();
    vppMock().coppTtlPuntBindCalls.clear();
    vppMock().coppTtlPuntBindFailOnCall = 1;
    vppMock().coppIfoutBindFailOnCall = 2;
    groupAttr.value.oid = newPolicerOid;
    EXPECT_EQ(SAI_STATUS_FAILURE, m_sw->setHostifTrapGroup(
            sai_serialize_object_id(trapGroupOid), &groupAttr));

    ASSERT_EQ(3u, vppMock().coppIfoutBindCalls.size());
    EXPECT_EQ("copp-policer-0x1200000004",
            vppMock().coppIfoutBindCalls[0].policerName);
    EXPECT_EQ("copp-policer-0x1200000002",
            vppMock().coppIfoutBindCalls[1].policerName);
    EXPECT_EQ("copp-policer-0x1200000002",
            vppMock().coppIfoutBindCalls[2].policerName);
    EXPECT_EQ(0u, m_sw->m_trap_binding_uncertain.size());
    EXPECT_EQ(oldPolicerOid,
            m_sw->m_trap_group_map.at(trapGroupOid).policer_oid);
}

TEST_F(SwitchVppNeighborMissTrap, TtlUninstallFailureRestoresActiveBinding)
{
    const sai_object_id_t trapGroupOid = 0x1200000001;
    const sai_object_id_t policerOid = 0x1200000002;
    const sai_object_id_t trapOid = 0x1200000003;

    sai_attribute_t groupAttr{};
    groupAttr.id = SAI_HOSTIF_TRAP_GROUP_ATTR_POLICER;
    groupAttr.value.oid = policerOid;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrapGroup(
            trapGroupOid, m_switchId, 1, &groupAttr));

    vpp_policer_entry_t policer{};
    policer.vpp_policer_index = 7;
    m_sw->m_policer_map[policerOid] = policer;

    sai_attribute_t trapAttrs[3]{};
    trapAttrs[0].id = SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE;
    trapAttrs[0].value.s32 = SAI_HOSTIF_TRAP_TYPE_TTL_ERROR;
    trapAttrs[1].id = SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION;
    trapAttrs[1].value.s32 = SAI_PACKET_ACTION_TRAP;
    trapAttrs[2].id = SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP;
    trapAttrs[2].value.oid = trapGroupOid;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrap(
            trapOid, m_switchId, 3, trapAttrs));

    vppMock().coppIfoutBindCalls.clear();
    vppMock().coppTtlPuntBindCalls.clear();
    vppMock().coppTtlPuntBindFailOnCall = 1;
    sai_attribute_t setAttr{};
    setAttr.id = SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION;
    setAttr.value.s32 = SAI_PACKET_ACTION_DROP;
    EXPECT_EQ(SAI_STATUS_FAILURE, m_sw->setHostifTrap(
            sai_serialize_object_id(trapOid), &setAttr));

    ASSERT_EQ(2u, vppMock().coppIfoutBindCalls.size());
    EXPECT_FALSE(vppMock().coppIfoutBindCalls[0].isBind);
    EXPECT_TRUE(vppMock().coppIfoutBindCalls[1].isBind);
    EXPECT_EQ("copp-policer-0x1200000002",
            vppMock().coppIfoutBindCalls[1].policerName);
    ASSERT_EQ(2u, vppMock().coppTtlPuntBindCalls.size());
    EXPECT_FALSE(vppMock().coppTtlPuntBindCalls[0]);
    EXPECT_TRUE(vppMock().coppTtlPuntBindCalls[1]);
    EXPECT_EQ(SAI_PACKET_ACTION_TRAP,
            m_sw->m_trap_map.at(trapOid).packet_action);
    EXPECT_TRUE(m_sw->m_trap_map.at(trapOid).classify_installed);
}

TEST_F(SwitchVppNeighborMissTrap, TrapSetBindFailureKeepsOldCachedState)
{
    const sai_object_id_t oldGroupOid = 0x1200000001;
    const sai_object_id_t oldPolicerOid = 0x1200000002;
    const sai_object_id_t trapOid = 0x1200000003;
    const sai_object_id_t newGroupOid = 0x1200000004;
    const sai_object_id_t newPolicerOid = 0x1200000005;

    sai_attribute_t groupAttr{};
    groupAttr.id = SAI_HOSTIF_TRAP_GROUP_ATTR_POLICER;
    groupAttr.value.oid = oldPolicerOid;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrapGroup(
            oldGroupOid, m_switchId, 1, &groupAttr));
    groupAttr.value.oid = newPolicerOid;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrapGroup(
            newGroupOid, m_switchId, 1, &groupAttr));

    vpp_policer_entry_t oldPolicer{};
    oldPolicer.vpp_policer_index = 7;
    m_sw->m_policer_map[oldPolicerOid] = oldPolicer;
    vpp_policer_entry_t newPolicer{};
    newPolicer.vpp_policer_index = 8;
    m_sw->m_policer_map[newPolicerOid] = newPolicer;

    sai_attribute_t trapAttrs[3]{};
    trapAttrs[0].id = SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE;
    trapAttrs[0].value.s32 = SAI_HOSTIF_TRAP_TYPE_NEIGHBOR_MISS;
    trapAttrs[1].id = SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION;
    trapAttrs[1].value.s32 = SAI_PACKET_ACTION_TRAP;
    trapAttrs[2].id = SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP;
    trapAttrs[2].value.oid = oldGroupOid;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrap(
            trapOid, m_switchId, 3, trapAttrs));

    vppMock().coppNeighborMissBindCalls.clear();
    vppMock().coppNeighborMissBindFailOnCall = 1;
    sai_attribute_t setAttr{};
    setAttr.id = SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP;
    setAttr.value.oid = newGroupOid;
    EXPECT_EQ(SAI_STATUS_FAILURE, m_sw->setHostifTrap(
            sai_serialize_object_id(trapOid), &setAttr));

    EXPECT_EQ(oldGroupOid, m_sw->m_trap_map.at(trapOid).trap_group_oid);
    sai_attribute_t readAttr{};
    readAttr.id = SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->get(
            SAI_OBJECT_TYPE_HOSTIF_TRAP, sai_serialize_object_id(trapOid),
            1, &readAttr));
    EXPECT_EQ(oldGroupOid, readAttr.value.oid);
}

TEST_F(SwitchVppNeighborMissTrap, DropRejectionDoesNotTouchActiveBinding)
{
    const sai_object_id_t trapGroupOid = 0x1200000001;
    const sai_object_id_t policerOid = 0x1200000002;
    const sai_object_id_t trapOid = 0x1200000003;

    sai_attribute_t groupAttr{};
    groupAttr.id = SAI_HOSTIF_TRAP_GROUP_ATTR_POLICER;
    groupAttr.value.oid = policerOid;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrapGroup(
            trapGroupOid, m_switchId, 1, &groupAttr));

    vpp_policer_entry_t policer{};
    policer.vpp_policer_index = 7;
    m_sw->m_policer_map[policerOid] = policer;

    sai_attribute_t trapAttrs[3]{};
    trapAttrs[0].id = SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE;
    trapAttrs[0].value.s32 = SAI_HOSTIF_TRAP_TYPE_NEIGHBOR_MISS;
    trapAttrs[1].id = SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION;
    trapAttrs[1].value.s32 = SAI_PACKET_ACTION_TRAP;
    trapAttrs[2].id = SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP;
    trapAttrs[2].value.oid = trapGroupOid;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->createHostifTrap(
            trapOid, m_switchId, 3, trapAttrs));

    vppMock().coppNeighborMissBindCalls.clear();
    vppMock().coppNeighborMissBindFailOnCall = 1;
    sai_attribute_t setAttr{};
    setAttr.id = SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION;
    setAttr.value.s32 = SAI_PACKET_ACTION_DROP;
    EXPECT_EQ(SAI_STATUS_NOT_SUPPORTED, m_sw->setHostifTrap(
            sai_serialize_object_id(trapOid), &setAttr));

    EXPECT_TRUE(vppMock().coppNeighborMissBindCalls.empty());
    EXPECT_EQ(SAI_PACKET_ACTION_TRAP,
            m_sw->m_trap_map.at(trapOid).packet_action);
    EXPECT_TRUE(m_sw->m_trap_map.at(trapOid).classify_installed);
    sai_attribute_t readAttr{};
    readAttr.id = SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->get(
            SAI_OBJECT_TYPE_HOSTIF_TRAP, sai_serialize_object_id(trapOid),
            1, &readAttr));
    EXPECT_EQ(SAI_PACKET_ACTION_TRAP, readAttr.value.s32);
}

/*
 * Two front panel ports, SONiC's Ethernet4 and Ethernet40 on VPP interfaces
 * PORT_HWIF and OTHER_PORT_HWIF, the first name a prefix of the second, each
 * behind a bridge port, and VLAN 100. A tagged member of the VLAN
 * is VPP sub-interface <port>.100 in bridge domain 100. A sub-port router
 * interface on VLAN 200 is <port>.200. Both ports start admin down in VPP, as
 * they do before orchagent sets SAI_PORT_ATTR_ADMIN_STATE.
 *
 * The unit tests are built with access control checking disabled, so the
 * fixture reaches the protected and private SwitchVpp members directly.
 */
class SwitchVppSubIfAdminState : public ::testing::Test
{
    protected:

        void SetUp() override
        {
            vppMock().reset();
            auto sc = std::make_shared<SwitchConfig>(0, "");

            sc->m_saiSwitchType = SAI_SWITCH_TYPE_NPU;
            sc->m_switchType = SAI_VS_SWITCH_TYPE_VPP;
            sc->m_bootType = SAI_VS_BOOT_TYPE_COLD;
            sc->m_useTapDevice = false;
            sc->m_profileMap[SAI_KEY_VS_PORT_CONFIG_FILE] = "/dev/null";
            sc->m_laneMap = LaneMap::getDefaultLaneMap(0);
            sc->m_eventQueue = std::make_shared<EventQueue>(std::make_shared<Signal>());

            auto scc = std::make_shared<SwitchConfigContainer>();

            scc->insert(sc);

            m_mgr = std::make_shared<RealObjectIdManager>(0, scc);
            m_sw = std::make_shared<TestableSwitchVpp>(m_switchId, m_mgr, sc);

            sc->m_useTapDevice = true;

            m_vlan = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_VLAN, m_switchId);

            sai_attribute_t vattr;

            vattr.id = SAI_VLAN_ATTR_VLAN_ID;
            vattr.value.u16 = VLAN_ID;

            ASSERT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create_internal(SAI_OBJECT_TYPE_VLAN, sai_serialize_object_id(m_vlan), m_switchId, 1, &vattr));

            addPort(PORT_HWIF, "Ethernet4", m_port, m_bridgePort);
            addPort(OTHER_PORT_HWIF, "Ethernet40", m_otherPort, m_otherBridgePort);

            vppMock().calls.clear();
            vppMock().adminUp.clear();
            vppMock().interfaces.clear();
        }

        void TearDown() override
        {
            m_sw->m_switchConfig->m_useTapDevice = false;
            m_sw.reset();
        }

        void addPort(
                const std::string& hwif,
                const std::string& osif,
                sai_object_id_t& port,
                sai_object_id_t& bridgePort)
        {
            SWSS_LOG_ENTER();

            port = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_PORT, m_switchId);

            sai_attribute_t pattr;

            pattr.id = SAI_PORT_ATTR_ADMIN_STATE;
            pattr.value.booldata = false;

            ASSERT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create_internal(SAI_OBJECT_TYPE_PORT, sai_serialize_object_id(port), m_switchId, 1, &pattr));

            // what resolving the port by its lanes records
            ASSERT_NE(nullptr, m_sw->m_ifaceRegistry.addPhysicalPort(hwif, osif, port));

            bridgePort = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_BRIDGE_PORT, m_switchId);

            sai_attribute_t battrs[2];

            battrs[0].id = SAI_BRIDGE_PORT_ATTR_TYPE;
            battrs[0].value.s32 = SAI_BRIDGE_PORT_TYPE_PORT;
            battrs[1].id = SAI_BRIDGE_PORT_ATTR_PORT_ID;
            battrs[1].value.oid = port;

            ASSERT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create_internal(SAI_OBJECT_TYPE_BRIDGE_PORT, sai_serialize_object_id(bridgePort), m_switchId, 2, battrs));
        }

        // orchagent adds the port to the VLAN as a tagged member
        sai_object_id_t addTaggedMember(
                sai_object_id_t bridgePort)
        {
            SWSS_LOG_ENTER();

            auto member = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_VLAN_MEMBER, m_switchId);

            sai_attribute_t attrs[3];

            attrs[0].id = SAI_VLAN_MEMBER_ATTR_VLAN_ID;
            attrs[0].value.oid = m_vlan;
            attrs[1].id = SAI_VLAN_MEMBER_ATTR_BRIDGE_PORT_ID;
            attrs[1].value.oid = bridgePort;
            attrs[2].id = SAI_VLAN_MEMBER_ATTR_VLAN_TAGGING_MODE;
            attrs[2].value.s32 = SAI_VLAN_TAGGING_MODE_TAGGED;

            EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->createVlanMember(member, m_switchId, 3, attrs));

            return member;
        }

        // orchagent sets SAI_PORT_ATTR_ADMIN_STATE
        void setPortAdminState(
                sai_object_id_t port,
                bool is_up)
        {
            SWSS_LOG_ENTER();

            sai_attribute_t attr;

            attr.id = SAI_PORT_ATTR_ADMIN_STATE;
            attr.value.booldata = is_up;

            EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->setPort(port, &attr));
        }

        // a sub-port router interface on VLAN 200 sets its admin state
        void setSubPortAdminState(
                bool is_up)
        {
            SWSS_LOG_ENTER();

            EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->vpp_set_interface_state(m_port, SUB_PORT_VLAN_ID, is_up));
        }

        static constexpr uint16_t VLAN_ID = 100;
        static constexpr uint16_t SUB_PORT_VLAN_ID = 200;
        static constexpr const char *PORT_HWIF = "bobm1";
        static constexpr const char *OTHER_PORT_HWIF = "bobm10";
        static constexpr const char *MEMBER_HWIF = "bobm1.100";
        static constexpr const char *OTHER_MEMBER_HWIF = "bobm10.100";
        static constexpr const char *SUB_PORT_HWIF = "bobm1.200";

        const sai_object_id_t m_switchId = 0x2100000000;

        std::shared_ptr<RealObjectIdManager> m_mgr;

        std::shared_ptr<SwitchVpp> m_sw;

        sai_object_id_t m_vlan = SAI_NULL_OBJECT_ID;
        sai_object_id_t m_port = SAI_NULL_OBJECT_ID;
        sai_object_id_t m_bridgePort = SAI_NULL_OBJECT_ID;
        sai_object_id_t m_otherPort = SAI_NULL_OBJECT_ID;
        sai_object_id_t m_otherBridgePort = SAI_NULL_OBJECT_ID;
};

constexpr uint16_t SwitchVppSubIfAdminState::VLAN_ID;
constexpr uint16_t SwitchVppSubIfAdminState::SUB_PORT_VLAN_ID;
constexpr const char *SwitchVppSubIfAdminState::PORT_HWIF;
constexpr const char *SwitchVppSubIfAdminState::OTHER_PORT_HWIF;
constexpr const char *SwitchVppSubIfAdminState::MEMBER_HWIF;
constexpr const char *SwitchVppSubIfAdminState::OTHER_MEMBER_HWIF;
constexpr const char *SwitchVppSubIfAdminState::SUB_PORT_HWIF;

/*
 * A tagged member added while its port is still admin down: VPP refuses the
 * sub-interface's admin up, and nothing brought it up when the port came up,
 * so the member stayed down in VPP while CONFIG_DB, APPL_DB and STATE_DB all
 * said up.
 */
TEST_F(SwitchVppSubIfAdminState, TaggedMemberAddedWhileThePortIsDownComesUpWithThePort)
{
    addTaggedMember(m_bridgePort);

    auto calls = vppMock().callsTo("interface_set_state", MEMBER_HWIF);

    ASSERT_EQ(1u, calls.size());
    EXPECT_TRUE(calls[0].flag);
    EXPECT_NE(0u, calls[0].sub_id);
    EXPECT_FALSE(vppMock().isAdminUp(MEMBER_HWIF));

    setPortAdminState(m_port, true);

    EXPECT_TRUE(vppMock().isAdminUp(PORT_HWIF));
    EXPECT_TRUE(vppMock().isAdminUp(MEMBER_HWIF));

    calls = vppMock().callsTo("interface_set_state", MEMBER_HWIF);

    ASSERT_EQ(2u, calls.size());
    EXPECT_TRUE(calls[1].flag);
    EXPECT_EQ(0u, calls[1].sub_id);

    // applied once: a later port flap doesn't set it again
    setPortAdminState(m_port, false);
    setPortAdminState(m_port, true);

    EXPECT_EQ(2u, vppMock().callsTo("interface_set_state", MEMBER_HWIF).size());
}

TEST_F(SwitchVppSubIfAdminState, TaggedMemberAddedWhileThePortIsUpIsSetUpOnce)
{
    setPortAdminState(m_port, true);

    addTaggedMember(m_bridgePort);

    auto calls = vppMock().callsTo("interface_set_state", MEMBER_HWIF);

    ASSERT_EQ(1u, calls.size());
    EXPECT_TRUE(calls[0].flag);
    EXPECT_EQ(0u, calls[0].sub_id);
    EXPECT_TRUE(vppMock().isAdminUp(MEMBER_HWIF));

    // VPP keeps the sub-interface admin up across a port flap on its own
    setPortAdminState(m_port, false);
    setPortAdminState(m_port, true);

    EXPECT_EQ(1u, vppMock().callsTo("interface_set_state", MEMBER_HWIF).size());
    EXPECT_TRUE(vppMock().isAdminUp(MEMBER_HWIF));
}

TEST_F(SwitchVppSubIfAdminState, TaggedMemberRemovedBeforeThePortComesUpIsNotSetUp)
{
    auto member = addTaggedMember(m_bridgePort);

    EXPECT_FALSE(vppMock().isAdminUp(MEMBER_HWIF));

    EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->removeVlanMember(member));

    ASSERT_EQ(1u, vppMock().callsTo("delete_sub_interface", PORT_HWIF).size());

    vppMock().calls.clear();

    setPortAdminState(m_port, true);

    EXPECT_TRUE(vppMock().isAdminUp(PORT_HWIF));
    EXPECT_TRUE(vppMock().callsTo("interface_set_state", MEMBER_HWIF).empty());
}

/*
 * A port coming up must not touch the sub-interfaces of another port whose
 * name it is a prefix of.
 */
TEST_F(SwitchVppSubIfAdminState, OnlyThePortThatComesUpBringsItsMembersUp)
{
    addTaggedMember(m_bridgePort);
    addTaggedMember(m_otherBridgePort);

    setPortAdminState(m_port, true);

    EXPECT_TRUE(vppMock().isAdminUp(MEMBER_HWIF));
    EXPECT_FALSE(vppMock().isAdminUp(OTHER_MEMBER_HWIF));
    EXPECT_EQ(1u, vppMock().callsTo("interface_set_state", OTHER_MEMBER_HWIF).size());

    setPortAdminState(m_otherPort, true);

    EXPECT_TRUE(vppMock().isAdminUp(OTHER_MEMBER_HWIF));
}

/*
 * The router interface path: SAI_ROUTER_INTERFACE_ATTR_ADMIN_V4_STATE or
 * ADMIN_V6_STATE on a sub-port.
 */
TEST_F(SwitchVppSubIfAdminState, SubPortSetUpWhileThePortIsDownComesUpWithThePort)
{
    setSubPortAdminState(true);

    EXPECT_FALSE(vppMock().isAdminUp(SUB_PORT_HWIF));

    setPortAdminState(m_port, true);

    EXPECT_TRUE(vppMock().isAdminUp(SUB_PORT_HWIF));
}

TEST_F(SwitchVppSubIfAdminState, SubPortSetDownBeforeThePortComesUpStaysDown)
{
    setSubPortAdminState(true);
    setSubPortAdminState(false);

    auto calls = vppMock().callsTo("interface_set_state", SUB_PORT_HWIF);

    ASSERT_EQ(2u, calls.size());
    EXPECT_FALSE(calls[1].flag);
    EXPECT_EQ(0u, calls[1].sub_id);

    setPortAdminState(m_port, true);

    EXPECT_EQ(2u, vppMock().callsTo("interface_set_state", SUB_PORT_HWIF).size());
    EXPECT_FALSE(vppMock().isAdminUp(SUB_PORT_HWIF));
}

TEST(SwitchVpp, fdbResyncRetriesFailedL2fibDump)
{
    vppMock().reset();

    auto sc = std::make_shared<SwitchConfig>(0, "");

    sc->m_saiSwitchType = SAI_SWITCH_TYPE_NPU;
    sc->m_switchType = SAI_VS_SWITCH_TYPE_VPP;
    sc->m_bootType = SAI_VS_BOOT_TYPE_COLD;
    sc->m_useTapDevice = false;
    sc->m_laneMap = LaneMap::getDefaultLaneMap();
    sc->m_eventQueue = std::make_shared<EventQueue>(std::make_shared<Signal>());

    auto scc = std::make_shared<SwitchConfigContainer>();
    scc->insert(sc);

    auto ridmgr = std::make_shared<RealObjectIdManager>(0, scc);
    auto swid = ridmgr->allocateNewSwitchObjectId("");

    SwitchVpp sw(swid, ridmgr, sc);

    // FDB notifications go to the switch's FDB event callback
    sai_attribute_t attr;
    attr.id = SAI_SWITCH_ATTR_FDB_EVENT_NOTIFY;
    attr.value.ptr = reinterpret_cast<void *>(0x1);

    ASSERT_EQ(SAI_STATUS_SUCCESS, sw.create_internal(SAI_OBJECT_TYPE_SWITCH,
                sai_serialize_object_id(swid), swid, 1, &attr));

    // A MAC learned from VPP, whose DELETE VPP sent while the event socket was down
    SwitchVpp::VppFdbKey key = {{0x02, 0x11, 0x22, 0x33, 0x44, 0x55}, 1000};

    FdbInfo fi;
    fi.m_fdbEntry.switch_id = swid;
    fi.m_fdbEntry.bv_id = ridmgr->allocateNewObjectId(SAI_OBJECT_TYPE_VLAN, swid);
    memcpy(fi.m_fdbEntry.mac_address, key.mac, sizeof(sai_mac_t));
    fi.setVlanId(static_cast<sai_vlan_id_t>(key.bd_id));
    fi.setPortId(ridmgr->allocateNewObjectId(SAI_OBJECT_TYPE_PORT, swid));
    fi.setBridgePortId(ridmgr->allocateNewObjectId(SAI_OBJECT_TYPE_BRIDGE_PORT, swid));

    sai_attribute_t attrs[2];
    attrs[0].id = SAI_FDB_ENTRY_ATTR_TYPE;
    attrs[0].value.s32 = SAI_FDB_ENTRY_TYPE_DYNAMIC;
    attrs[1].id = SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID;
    attrs[1].value.oid = fi.getBridgePortId();

    auto fdbId = sai_serialize_fdb_entry(fi.getFdbEntry());
    auto &fdbs = sw.m_objectHash.at(SAI_OBJECT_TYPE_FDB_ENTRY);

    ASSERT_EQ(SAI_STATUS_SUCCESS, sw.create_internal(SAI_OBJECT_TYPE_FDB_ENTRY, fdbId, swid, 2, attrs));
    sw.m_fdb_info_set.insert(fi);
    sw.m_vpp_fdb_entries[key] = 5;

    // The event socket reconnected
    sw.m_mac_event_queue.push({{0}, ~0u, VPP_MAC_ACTION_RESYNC});

    // The L2FIB dump fails: nothing changes and the resync stays pending
    vppMock().l2fibDumpResult = -1;

    sw.processFdbEntriesForAging();

    EXPECT_EQ(1u, vppMock().l2fibDumpCalls);
    EXPECT_EQ(1u, sw.m_vpp_fdb_entries.count(key));
    EXPECT_EQ(1u, sw.m_fdb_info_set.size());
    EXPECT_EQ(1u, fdbs.count(fdbId));
    EXPECT_EQ(0u, sc->m_eventQueue->size());
    ASSERT_EQ(1u, sw.m_mac_event_queue.size());
    EXPECT_EQ(static_cast<uint8_t>(VPP_MAC_ACTION_RESYNC), sw.m_mac_event_queue.front().action);

    // The next aging pass dumps the L2FIB, which no longer has the MAC: it is aged out
    vppMock().l2fibDumpResult = 0;

    sw.processFdbEntriesForAging();

    EXPECT_EQ(2u, vppMock().l2fibDumpCalls);
    EXPECT_TRUE(sw.m_mac_event_queue.empty());
    EXPECT_TRUE(sw.m_vpp_fdb_entries.empty());
    EXPECT_TRUE(sw.m_fdb_info_set.empty());
    EXPECT_TRUE(fdbs.empty());

    ASSERT_EQ(1u, sc->m_eventQueue->size());

    auto event = sc->m_eventQueue->dequeue();

    ASSERT_EQ(EVENT_TYPE_NOTIFICATION, event->getType());

    auto ntf = std::dynamic_pointer_cast<EventPayloadNotification>(event->getPayload())->getNotification();

    EXPECT_EQ(SAI_SWITCH_NOTIFICATION_TYPE_FDB_EVENT, ntf->getNotificationType());
    EXPECT_NE(std::string::npos, ntf->getSerializedNotification().find("SAI_FDB_EVENT_AGED"));
    EXPECT_NE(std::string::npos, ntf->getSerializedNotification().find("02:11:22:33:44:55"));
}

TEST(SwitchVpp, fdbResyncSkipsL2fibRowsFromAnEarlierBd)
{
    vppMock().reset();

    auto sc = std::make_shared<SwitchConfig>(0, "");

    sc->m_saiSwitchType = SAI_SWITCH_TYPE_NPU;
    sc->m_switchType = SAI_VS_SWITCH_TYPE_VPP;
    sc->m_bootType = SAI_VS_BOOT_TYPE_COLD;
    sc->m_useTapDevice = false;
    sc->m_laneMap = LaneMap::getDefaultLaneMap();
    sc->m_eventQueue = std::make_shared<EventQueue>(std::make_shared<Signal>());

    auto scc = std::make_shared<SwitchConfigContainer>();
    scc->insert(sc);

    auto ridmgr = std::make_shared<RealObjectIdManager>(0, scc);
    auto swid = ridmgr->allocateNewSwitchObjectId("");

    SwitchVpp sw(swid, ridmgr, sc);

    sai_attribute_t attr;
    attr.id = SAI_SWITCH_ATTR_FDB_EVENT_NOTIFY;
    attr.value.ptr = reinterpret_cast<void *>(0x1);

    ASSERT_EQ(SAI_STATUS_SUCCESS, sw.create_internal(SAI_OBJECT_TYPE_SWITCH,
                sai_serialize_object_id(swid), swid, 1, &attr));

    // A port that has just moved from VLAN 100 to VLAN 200
    auto port = ridmgr->allocateNewObjectId(SAI_OBJECT_TYPE_PORT, swid);

    ASSERT_NE(nullptr, sw.m_ifaceRegistry.addPhysicalPort("bobm1", "Ethernet4", port));
    ASSERT_TRUE(sw.m_ifaceRegistry.bindSwIfIndex("bobm1", 5));
    ASSERT_TRUE(sw.m_ifaceRegistry.setBdId("bobm1", 200));

    // A MAC learned on it in VLAN 200, whose DELETE VPP sent while the event socket was down
    SwitchVpp::VppFdbKey key = {{0x02, 0x11, 0x22, 0x33, 0x44, 0x55}, 200};

    FdbInfo fi;
    fi.m_fdbEntry.switch_id = swid;
    fi.m_fdbEntry.bv_id = ridmgr->allocateNewObjectId(SAI_OBJECT_TYPE_VLAN, swid);
    memcpy(fi.m_fdbEntry.mac_address, key.mac, sizeof(sai_mac_t));
    fi.setVlanId(static_cast<sai_vlan_id_t>(key.bd_id));
    fi.setPortId(port);
    fi.setBridgePortId(ridmgr->allocateNewObjectId(SAI_OBJECT_TYPE_BRIDGE_PORT, swid));

    sai_attribute_t attrs[2];
    attrs[0].id = SAI_FDB_ENTRY_ATTR_TYPE;
    attrs[0].value.s32 = SAI_FDB_ENTRY_TYPE_DYNAMIC;
    attrs[1].id = SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID;
    attrs[1].value.oid = fi.getBridgePortId();

    auto fdbId = sai_serialize_fdb_entry(fi.getFdbEntry());
    auto &fdbs = sw.m_objectHash.at(SAI_OBJECT_TYPE_FDB_ENTRY);

    ASSERT_EQ(SAI_STATUS_SUCCESS, sw.create_internal(SAI_OBJECT_TYPE_FDB_ENTRY, fdbId, swid, 2, attrs));
    sw.m_fdb_info_set.insert(fi);
    sw.m_vpp_fdb_entries[key] = 5;

    // VPP's only row for the MAC is from VLAN 100: flushed when the port left
    // that VLAN, and not yet deleted by VPP's aging scan
    vpp_l2fib_entry_t row = {};
    memcpy(row.mac, key.mac, sizeof(row.mac));
    row.bd_id = 100;
    row.sw_if_index = 5;

    vppMock().l2fib = {row};

    // The event socket reconnected: that row does not keep the MAC in VLAN 200
    sw.m_mac_event_queue.push({{0}, ~0u, VPP_MAC_ACTION_RESYNC});
    sw.processFdbEntriesForAging();

    EXPECT_EQ(1u, vppMock().l2fibDumpCalls);
    EXPECT_TRUE(sw.m_mac_event_queue.empty());
    EXPECT_TRUE(sw.m_vpp_fdb_entries.empty());
    EXPECT_TRUE(sw.m_fdb_info_set.empty());
    EXPECT_TRUE(fdbs.empty());

    ASSERT_EQ(1u, sc->m_eventQueue->size());

    auto event = sc->m_eventQueue->dequeue();

    ASSERT_EQ(EVENT_TYPE_NOTIFICATION, event->getType());

    auto ntf = std::dynamic_pointer_cast<EventPayloadNotification>(event->getPayload())->getNotification();

    EXPECT_EQ(SAI_SWITCH_NOTIFICATION_TYPE_FDB_EVENT, ntf->getNotificationType());
    EXPECT_NE(std::string::npos, ntf->getSerializedNotification().find("SAI_FDB_EVENT_AGED"));
    EXPECT_NE(std::string::npos, ntf->getSerializedNotification().find("02:11:22:33:44:55"));
}

TEST(SwitchVpp, bfdStateEventAppliedOnEventQueueThread)
{
    auto sc = std::make_shared<SwitchConfig>(0, "");

    sc->m_saiSwitchType = SAI_SWITCH_TYPE_NPU;
    sc->m_switchType = SAI_VS_SWITCH_TYPE_VPP;
    sc->m_bootType = SAI_VS_BOOT_TYPE_COLD;
    sc->m_useTapDevice = false;
    sc->m_laneMap = LaneMap::getDefaultLaneMap();
    sc->m_eventQueue = std::make_shared<EventQueue>(std::make_shared<Signal>());

    auto scc = std::make_shared<SwitchConfigContainer>();
    scc->insert(sc);

    auto ridmgr = std::make_shared<RealObjectIdManager>(0, scc);
    auto swid = ridmgr->allocateNewSwitchObjectId("");

    SwitchVpp sw(swid, ridmgr, sc);

    // BFD notifications go to the switch's BFD session state change callback
    sai_attribute_t attr;
    attr.id = SAI_SWITCH_ATTR_BFD_SESSION_STATE_CHANGE_NOTIFY;
    attr.value.ptr = reinterpret_cast<void *>(0x1);

    ASSERT_EQ(SAI_STATUS_SUCCESS, sw.create_internal(SAI_OBJECT_TYPE_SWITCH,
                sai_serialize_object_id(swid), swid, 1, &attr));

    // A BFD session that is DOWN
    auto bfdOid = ridmgr->allocateNewObjectId(SAI_OBJECT_TYPE_BFD_SESSION, swid);

    attr.id = SAI_BFD_SESSION_ATTR_STATE;
    attr.value.s32 = SAI_BFD_SESSION_STATE_DOWN;

    ASSERT_EQ(SAI_STATUS_SUCCESS, sw.create_internal(SAI_OBJECT_TYPE_BFD_SESSION,
                sai_serialize_object_id(bfdOid), swid, 1, &attr));

    auto bfdState = [&sw, bfdOid]()
    {
        sai_attribute_t stateAttr;
        stateAttr.id = SAI_BFD_SESSION_ATTR_STATE;

        EXPECT_EQ(SAI_STATUS_SUCCESS, sw.get(SAI_OBJECT_TYPE_BFD_SESSION, bfdOid, 1, &stateAttr));

        return static_cast<sai_bfd_session_state_t>(stateAttr.value.s32);
    };

    // VPP reports the session UP
    auto *ev = static_cast<vpp_event_info_t *>(calloc(1, sizeof(vpp_event_info_t)));

    ASSERT_NE(nullptr, ev);

    auto &notif = ev->data.bfd_notif;

    ev->type = VPP_BFD_STATE_CHANGE;
    notif.state = VPP_API_BFD_STATE_UP;
    notif.local_addr.sa_family = AF_INET;
    notif.peer_addr.sa_family = AF_INET;

    ASSERT_EQ(1, inet_pton(AF_INET, "10.0.0.1", &notif.local_addr.addr.ip4.sin_addr));
    ASSERT_EQ(1, inet_pton(AF_INET, "10.0.0.2", &notif.peer_addr.addr.ip4.sin_addr));

    SwitchVpp::vpp_bfd_info_t key;
    memset(&key, 0, sizeof(key));
    vpp_ip_addr_t_to_sai_ip_address_t(notif.local_addr, key.local_addr);
    vpp_ip_addr_t_to_sai_ip_address_t(notif.peer_addr, key.peer_addr);
    sw.m_bfd_info_map[key] = bfdOid;

    // The VPP events thread takes the event (and frees it)
    deliverVppEvents({ev});

    ASSERT_TRUE(waitForVppEventsTaken());

    // It touches neither Meta nor the switch state, which need m_apimutex: it
    // only posts the event, for the vslib event queue thread to apply
    EXPECT_EQ(SAI_BFD_SESSION_STATE_DOWN, bfdState());

    ASSERT_EQ(1u, sc->m_eventQueue->size());

    auto event = sc->m_eventQueue->dequeue();

    ASSERT_EQ(EVENT_TYPE_PLATFORM_EVENT, event->getType());

    auto payload = std::dynamic_pointer_cast<EventPayloadVppEvent>(event->getPayload());

    ASSERT_NE(nullptr, payload);
    EXPECT_EQ(swid, payload->getSwitchId());
    EXPECT_EQ(VPP_BFD_STATE_CHANGE, payload->getEvent().type);
    EXPECT_EQ(nullptr, payload->getEvent().next);

    // The event queue thread applies it (with m_apimutex held)
    sw.syncOnPlatformEvent(payload);

    EXPECT_EQ(SAI_BFD_SESSION_STATE_UP, bfdState());

    ASSERT_EQ(1u, sc->m_eventQueue->size());

    event = sc->m_eventQueue->dequeue();

    ASSERT_EQ(EVENT_TYPE_NOTIFICATION, event->getType());

    auto ntf = std::dynamic_pointer_cast<EventPayloadNotification>(event->getPayload())->getNotification();

    EXPECT_EQ(SAI_SWITCH_NOTIFICATION_TYPE_BFD_SESSION_STATE_CHANGE, ntf->getNotificationType());
    EXPECT_NE(std::string::npos, ntf->getSerializedNotification().find("SAI_BFD_SESSION_STATE_UP"));
}

TEST(SwitchVpp, syncOnPlatformEventDropsUnusableEvents)
{
    auto sc = std::make_shared<SwitchConfig>(0, "");

    sc->m_saiSwitchType = SAI_SWITCH_TYPE_NPU;
    sc->m_switchType = SAI_VS_SWITCH_TYPE_VPP;
    sc->m_bootType = SAI_VS_BOOT_TYPE_COLD;
    sc->m_useTapDevice = false;
    sc->m_laneMap = LaneMap::getDefaultLaneMap();
    sc->m_eventQueue = std::make_shared<EventQueue>(std::make_shared<Signal>());

    auto scc = std::make_shared<SwitchConfigContainer>();
    scc->insert(sc);

    auto ridmgr = std::make_shared<RealObjectIdManager>(0, scc);
    auto swid = ridmgr->allocateNewSwitchObjectId("");

    SwitchVpp sw(swid, ridmgr, sc);

    sai_attribute_t attr;
    attr.id = SAI_SWITCH_ATTR_PORT_STATE_CHANGE_NOTIFY;
    attr.value.ptr = reinterpret_cast<void *>(0x1);

    ASSERT_EQ(SAI_STATUS_SUCCESS, sw.create_internal(SAI_OBJECT_TYPE_SWITCH,
                sai_serialize_object_id(swid), swid, 1, &attr));

    // A platform event that is not a VPP event
    sw.syncOnPlatformEvent(std::make_shared<EventPayloadPlatformEvent>(swid));

    // A link event for an interface that is not a port
    vpp_event_info_t ev;
    memset(&ev, 0, sizeof(ev));

    ev.type = VPP_INTF_LINK_STATUS;
    strncpy(ev.data.intf_status.hwif_name, "bvi1000", sizeof(ev.data.intf_status.hwif_name) - 1);
    ev.data.intf_status.link_up = true;

    sw.syncOnPlatformEvent(std::make_shared<EventPayloadVppEvent>(swid, ev));

    EXPECT_EQ(0u, sc->m_eventQueue->size());
}

/*
 * Link events VPP sent while the event socket was down are lost, so a reconnect
 * queues a link state resync behind the events read before the failure. A stale
 * DOWN read before the failure must not win over that resync, which reads the
 * link back UP. And the resync compares with what SONiC was last told, not with
 * what an earlier resync read.
 */
TEST(SwitchVpp, linkResyncFollowsQueuedLinkEvents)
{
    vppMock().reset();

    auto sc = std::make_shared<SwitchConfig>(0, "");

    sc->m_saiSwitchType = SAI_SWITCH_TYPE_NPU;
    sc->m_switchType = SAI_VS_SWITCH_TYPE_VPP;
    sc->m_bootType = SAI_VS_BOOT_TYPE_COLD;
    sc->m_useTapDevice = false;
    sc->m_laneMap = LaneMap::getDefaultLaneMap();
    sc->m_eventQueue = std::make_shared<EventQueue>(std::make_shared<Signal>());

    auto scc = std::make_shared<SwitchConfigContainer>();
    scc->insert(sc);

    auto ridmgr = std::make_shared<RealObjectIdManager>(0, scc);
    auto swid = ridmgr->allocateNewSwitchObjectId("");

    SwitchVpp sw(swid, ridmgr, sc);

    sai_attribute_t attr;
    attr.id = SAI_SWITCH_ATTR_PORT_STATE_CHANGE_NOTIFY;
    attr.value.ptr = reinterpret_cast<void *>(0x1);

    ASSERT_EQ(SAI_STATUS_SUCCESS, sw.create_internal(SAI_OBJECT_TYPE_SWITCH,
                sai_serialize_object_id(swid), swid, 1, &attr));

    // Port Ethernet4 is UP, with its host interface and its VPP interface
    auto port = ridmgr->allocateNewObjectId(SAI_OBJECT_TYPE_PORT, swid);

    attr.id = SAI_PORT_ATTR_OPER_STATUS;
    attr.value.s32 = SAI_PORT_OPER_STATUS_UP;

    ASSERT_EQ(SAI_STATUS_SUCCESS, sw.create_internal(SAI_OBJECT_TYPE_PORT,
                sai_serialize_object_id(port), swid, 1, &attr));

    ASSERT_NE(nullptr, sw.m_ifaceRegistry.addPhysicalPort("bobm1", "Ethernet4", port));

    // what sonic_vpp_ifmap.ini maps the host interface to
    sw.m_ifaceRegistry.m_osIfToHwIf["Ethernet4"] = "bobm1";
    sw.m_ifaceRegistry.m_ifMapLoaded = true;

    sw.m_hostif_info_map["Ethernet4"] = std::make_shared<HostInterfaceInfo>(0, -1, -1, "Ethernet4", port, sc->m_eventQueue);

    vppMock().linkUp["bobm1"] = true;

    auto operStatus = [&sw, port]()
    {
        sai_attribute_t statusAttr;
        statusAttr.id = SAI_PORT_ATTR_OPER_STATUS;

        EXPECT_EQ(SAI_STATUS_SUCCESS, sw.get(SAI_OBJECT_TYPE_PORT, port, 1, &statusAttr));

        return static_cast<sai_port_oper_status_t>(statusAttr.value.s32);
    };

    auto vppEvent = [](vpp_event_type_e type)
    {
        auto *ev = static_cast<vpp_event_info_t *>(calloc(1, sizeof(vpp_event_info_t)));

        ev->type = type;

        return ev;
    };

    auto *linkDown = vppEvent(VPP_INTF_LINK_STATUS);

    snprintf(linkDown->data.intf_status.hwif_name, sizeof(linkDown->data.intf_status.hwif_name), "%s", "bobm1");
    linkDown->data.intf_status.link_up = false;

    // An earlier reconnect's resync finds nothing to fix. Then the event thread
    // reads a link DOWN, the event socket fails, the link comes back UP while it
    // is down, and the reconnect queues a resync behind the DOWN.
    deliverVppEvents({vppEvent(VPP_INTF_LINK_RESYNC), linkDown, vppEvent(VPP_INTF_LINK_RESYNC)});

    ASSERT_TRUE(waitForVppEventsTaken());

    // The event thread posts them in that order...
    std::vector<std::shared_ptr<EventPayloadVppEvent>> posted;

    while (auto event = sc->m_eventQueue->dequeue())
    {
        ASSERT_EQ(EVENT_TYPE_PLATFORM_EVENT, event->getType());

        posted.push_back(std::dynamic_pointer_cast<EventPayloadVppEvent>(event->getPayload()));
    }

    ASSERT_EQ(3u, posted.size());
    EXPECT_EQ(VPP_INTF_LINK_RESYNC, posted[0]->getEvent().type);
    EXPECT_EQ(VPP_INTF_LINK_STATUS, posted[1]->getEvent().type);
    EXPECT_EQ(VPP_INTF_LINK_RESYNC, posted[2]->getEvent().type);

    // ...and the event queue thread applies them in that order
    std::vector<std::string> notified;

    auto apply = [&sw, &sc, &notified](std::shared_ptr<EventPayloadVppEvent> payload)
    {
        sw.syncOnPlatformEvent(payload);

        while (auto event = sc->m_eventQueue->dequeue())
        {
            EXPECT_EQ(EVENT_TYPE_NOTIFICATION, event->getType());

            auto ntf = std::dynamic_pointer_cast<EventPayloadNotification>(event->getPayload())->getNotification();

            EXPECT_EQ(SAI_SWITCH_NOTIFICATION_TYPE_PORT_STATE_CHANGE, ntf->getNotificationType());

            notified.push_back(ntf->getSerializedNotification());
        }
    };

    apply(posted[0]);

    EXPECT_TRUE(notified.empty());
    EXPECT_EQ(SAI_PORT_OPER_STATUS_UP, operStatus());

    apply(posted[1]);

    EXPECT_EQ(SAI_PORT_OPER_STATUS_DOWN, operStatus());

    apply(posted[2]);

    EXPECT_EQ(SAI_PORT_OPER_STATUS_UP, operStatus());

    ASSERT_EQ(2u, notified.size());
    EXPECT_NE(std::string::npos, notified[0].find("SAI_PORT_OPER_STATUS_DOWN"));
    EXPECT_NE(std::string::npos, notified[1].find("SAI_PORT_OPER_STATUS_UP"));
}

namespace
{
    std::shared_ptr<SwitchConfig> vppSwitchConfig()
    {
        SWSS_LOG_ENTER();

        auto sc = std::make_shared<SwitchConfig>(0, "");

        sc->m_saiSwitchType = SAI_SWITCH_TYPE_NPU;
        sc->m_switchType = SAI_VS_SWITCH_TYPE_VPP;
        sc->m_bootType = SAI_VS_BOOT_TYPE_COLD;
        sc->m_useTapDevice = false;
        sc->m_laneMap = LaneMap::getDefaultLaneMap();
        sc->m_eventQueue = std::make_shared<EventQueue>(std::make_shared<Signal>());

        return sc;
    }
}

/*
 * The event thread reads what VPP has sent as soon as it wakes up for it, and
 * pings VPP, to check the event socket, only after an interval with nothing to
 * read.
 */
TEST(SwitchVpp, eventThreadPingsOnlyAfterAQuietInterval)
{
    resetVppEventCalls();

    // An event is waiting when the event thread starts
    auto *ev = static_cast<vpp_event_info_t *>(calloc(1, sizeof(vpp_event_info_t)));

    ASSERT_NE(nullptr, ev);

    ev->type = VPP_INTF_LINK_RESYNC;

    deliverVppEvents({ev});

    {
        auto sc = vppSwitchConfig();
        auto scc = std::make_shared<SwitchConfigContainer>();
        scc->insert(sc);

        auto ridmgr = std::make_shared<RealObjectIdManager>(0, scc);

        SwitchVpp sw(ridmgr->allocateNewSwitchObjectId(""), ridmgr, sc);

        ASSERT_TRUE(waitForVppEventsTaken());
    }

    // The switch is gone, and its event thread with it
    auto calls = vppEventCalls();

    unsigned int drains = 0;
    unsigned int pings = 0;

    for (size_t i = 0; i < calls.size(); i++)
    {
        if (calls[i].api != "vpp_wait_for_events")
        {
            continue;
        }

        ASSERT_LT(i + 1, calls.size());

        if (calls[i].result > 0)
        {
            EXPECT_EQ("vpp_drain_events", calls[i + 1].api);
            drains++;
        }
        else
        {
            EXPECT_EQ("vpp_sync_for_events", calls[i + 1].api);
            pings++;
        }
    }

    EXPECT_EQ(1u, drains);
    EXPECT_LE(1u, pings);
}

/*
 * A failure of the event socket reconnects it. Were the next attempt to follow
 * at once, a socket that keeps failing would make the event thread spin: it
 * tries again only an interval later.
 */
TEST(SwitchVpp, eventThreadRetriesAFailedSocketAnIntervalLater)
{
    // The first ping fails
    resetVppEventCalls(1);

    auto calls = vppEventCalls();

    auto failed = [&calls]()
    {
        return std::find_if(calls.begin(), calls.end(),
                [](const VppEventCall& call) { return call.result < 0; });
    };

    {
        auto sc = vppSwitchConfig();
        auto scc = std::make_shared<SwitchConfigContainer>();
        scc->insert(sc);

        auto ridmgr = std::make_shared<RealObjectIdManager>(0, scc);

        SwitchVpp sw(ridmgr->allocateNewSwitchObjectId(""), ridmgr, sc);

        // Until the event thread makes its next call after the failure
        for (int i = 0; i < 500; i++)
        {
            calls = vppEventCalls();

            auto it = failed();

            if (it != calls.end() && it + 1 != calls.end())
            {
                break;
            }

            usleep(10 * 1000);
        }
    }

    auto it = failed();

    ASSERT_NE(calls.end(), it);
    ASSERT_NE(calls.end(), it + 1);

    // It pings again, which reconnects the socket if it is still down, an
    // interval later and without waiting on the socket in between
    EXPECT_EQ("vpp_sync_for_events", (it + 1)->api);
    EXPECT_LE(std::chrono::milliseconds(1900), (it + 1)->time - it->time);
}

/*
 * BFD events lost with the event socket, sent while it was down or still
 * buffered on it when it failed, leave a session's state stale: the reconnect
 * queues a BFD state resync, which reports the sessions whose state VPP has
 * changed since. If VPP's sessions can't be dumped, the FDB aging pass retries.
 */
TEST(SwitchVpp, bfdResyncReportsStateChangesLostWithTheEventSocket)
{
    vppMock().reset();

    auto sc = vppSwitchConfig();
    auto scc = std::make_shared<SwitchConfigContainer>();
    scc->insert(sc);

    auto ridmgr = std::make_shared<RealObjectIdManager>(0, scc);
    auto swid = ridmgr->allocateNewSwitchObjectId("");

    SwitchVpp sw(swid, ridmgr, sc);

    sai_attribute_t attr;
    attr.id = SAI_SWITCH_ATTR_BFD_SESSION_STATE_CHANGE_NOTIFY;
    attr.value.ptr = reinterpret_cast<void *>(0x1);

    ASSERT_EQ(SAI_STATUS_SUCCESS, sw.create_internal(SAI_OBJECT_TYPE_SWITCH,
                sai_serialize_object_id(swid), swid, 1, &attr));

    // A BFD session, UP in SONiC or without a state change yet, and how VPP reports it
    auto addSession = [&](const char *peer, bool up, vpp_api_bfd_state_e vppState)
    {
        auto oid = ridmgr->allocateNewObjectId(SAI_OBJECT_TYPE_BFD_SESSION, swid);

        sai_attribute_t stateAttr;
        stateAttr.id = SAI_BFD_SESSION_ATTR_STATE;
        stateAttr.value.s32 = SAI_BFD_SESSION_STATE_UP;

        EXPECT_EQ(SAI_STATUS_SUCCESS, sw.create_internal(SAI_OBJECT_TYPE_BFD_SESSION,
                    sai_serialize_object_id(oid), swid, up ? 1 : 0, &stateAttr));

        vpp_bfd_state_notif_t session;
        memset(&session, 0, sizeof(session));

        session.multihop = true;
        session.sw_if_index = ~0u;
        session.state = vppState;
        session.local_addr.sa_family = AF_INET;
        session.peer_addr.sa_family = AF_INET;

        EXPECT_EQ(1, inet_pton(AF_INET, "10.0.0.1", &session.local_addr.addr.ip4.sin_addr));
        EXPECT_EQ(1, inet_pton(AF_INET, peer, &session.peer_addr.addr.ip4.sin_addr));

        SwitchVpp::vpp_bfd_info_t key;
        memset(&key, 0, sizeof(key));
        key.multihop = true;
        vpp_ip_addr_t_to_sai_ip_address_t(session.local_addr, key.local_addr);
        vpp_ip_addr_t_to_sai_ip_address_t(session.peer_addr, key.peer_addr);
        sw.m_bfd_info_map[key] = oid;

        vppMock().bfdSessions.push_back(session);

        return oid;
    };

    // While the event socket was down, VPP took a session DOWN and a new one
    // UP. It still has another session UP, and a new one DOWN.
    auto wentDown = addSession("10.0.0.2", true, VPP_API_BFD_STATE_DOWN);
    auto stayedUp = addSession("10.0.0.3", true, VPP_API_BFD_STATE_UP);
    auto cameUp = addSession("10.0.0.4", false, VPP_API_BFD_STATE_UP);
    auto stayedDown = addSession("10.0.0.5", false, VPP_API_BFD_STATE_DOWN);

    auto bfdState = [&sw](sai_object_id_t oid)
    {
        sai_attribute_t stateAttr;
        stateAttr.id = SAI_BFD_SESSION_ATTR_STATE;

        EXPECT_EQ(SAI_STATUS_SUCCESS, sw.get(SAI_OBJECT_TYPE_BFD_SESSION, oid, 1, &stateAttr));

        return static_cast<sai_bfd_session_state_t>(stateAttr.value.s32);
    };

    vpp_event_info_t resync;
    memset(&resync, 0, sizeof(resync));
    resync.type = VPP_BFD_RESYNC;

    // If the session dump fails, nothing changes, and the next FDB aging pass
    // tries again
    vppMock().bfdDumpResult = -1;

    sw.syncOnPlatformEvent(std::make_shared<EventPayloadVppEvent>(swid, resync));

    EXPECT_EQ(1u, vppMock().bfdDumpCalls);
    EXPECT_EQ(SAI_BFD_SESSION_STATE_UP, bfdState(wentDown));
    EXPECT_EQ(0u, sc->m_eventQueue->size());

    sw.processFdbEntriesForAging();

    EXPECT_EQ(2u, vppMock().bfdDumpCalls);
    EXPECT_EQ(SAI_BFD_SESSION_STATE_UP, bfdState(wentDown));
    EXPECT_EQ(0u, sc->m_eventQueue->size());

    // Once the dump succeeds, the resync reports the sessions whose state
    // changed, and only those
    vppMock().bfdDumpResult = 0;

    sw.processFdbEntriesForAging();

    EXPECT_EQ(3u, vppMock().bfdDumpCalls);
    EXPECT_EQ(SAI_BFD_SESSION_STATE_DOWN, bfdState(wentDown));
    EXPECT_EQ(SAI_BFD_SESSION_STATE_UP, bfdState(stayedUp));
    EXPECT_EQ(SAI_BFD_SESSION_STATE_UP, bfdState(cameUp));

    std::vector<std::string> notified;

    while (auto event = sc->m_eventQueue->dequeue())
    {
        ASSERT_EQ(EVENT_TYPE_NOTIFICATION, event->getType());

        auto ntf = std::dynamic_pointer_cast<EventPayloadNotification>(event->getPayload())->getNotification();

        EXPECT_EQ(SAI_SWITCH_NOTIFICATION_TYPE_BFD_SESSION_STATE_CHANGE, ntf->getNotificationType());

        notified.push_back(ntf->getSerializedNotification());
    }

    ASSERT_EQ(2u, notified.size());
    EXPECT_NE(std::string::npos, notified[0].find(sai_serialize_object_id(wentDown)));
    EXPECT_NE(std::string::npos, notified[0].find("SAI_BFD_SESSION_STATE_DOWN"));
    EXPECT_NE(std::string::npos, notified[1].find(sai_serialize_object_id(cameUp)));
    EXPECT_NE(std::string::npos, notified[1].find("SAI_BFD_SESSION_STATE_UP"));

    for (const auto& ntf: notified)
    {
        EXPECT_EQ(std::string::npos, ntf.find(sai_serialize_object_id(stayedDown)));
    }

    // Done: the aging pass doesn't resync again
    sw.processFdbEntriesForAging();

    EXPECT_EQ(3u, vppMock().bfdDumpCalls);
    EXPECT_EQ(0u, sc->m_eventQueue->size());
}
