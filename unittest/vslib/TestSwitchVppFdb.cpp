#include "MockVppXlate.h"
#include "vpp/vppxlate/SaiVppXlate.h"
#include "EventPayloadNotification.h"
#include "SwitchConfigContainer.h"
#include "meta/sai_serialize.h"
#include "saivs.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include <unistd.h>

using namespace saivs;

/*
 * The FDB tests: MAC events from VPP, their resync, and FDB aging.
 *
 * SwitchVppFdbTest is a switch with VLAN 100 and two ports, each with a
 * bridge port, which are not VLAN members yet. The unit tests are built with
 * access control checking disabled, so the fixtures reach the protected and
 * private SwitchVpp members directly.
 */
class SwitchVppFdbTest : public ::testing::Test
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

        // orchagent adds the port to VLAN 100 as a tagged member
        sai_object_id_t addTaggedMember(
                sai_object_id_t bridgePort)
        {
            SWSS_LOG_ENTER();

            return addTaggedMember(bridgePort, m_vlan);
        }

        sai_object_id_t addTaggedMember(
                sai_object_id_t bridgePort,
                sai_object_id_t vlan)
        {
            SWSS_LOG_ENTER();

            auto member = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_VLAN_MEMBER, m_switchId);

            sai_attribute_t attrs[3];

            attrs[0].id = SAI_VLAN_MEMBER_ATTR_VLAN_ID;
            attrs[0].value.oid = vlan;
            attrs[1].id = SAI_VLAN_MEMBER_ATTR_BRIDGE_PORT_ID;
            attrs[1].value.oid = bridgePort;
            attrs[2].id = SAI_VLAN_MEMBER_ATTR_VLAN_TAGGING_MODE;
            attrs[2].value.s32 = SAI_VLAN_TAGGING_MODE_TAGGED;

            EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->createVlanMember(member, m_switchId, 3, attrs));

            return member;
        }

        static constexpr uint16_t VLAN_ID = 100;
        static constexpr const char *PORT_HWIF = "bobm1";
        static constexpr const char *OTHER_PORT_HWIF = "bobm10";
        static constexpr const char *MEMBER_HWIF = "bobm1.100";

        const sai_object_id_t m_switchId = 0x2100000000;

        std::shared_ptr<RealObjectIdManager> m_mgr;

        std::shared_ptr<SwitchVpp> m_sw;

        sai_object_id_t m_vlan = SAI_NULL_OBJECT_ID;
        sai_object_id_t m_port = SAI_NULL_OBJECT_ID;
        sai_object_id_t m_bridgePort = SAI_NULL_OBJECT_ID;
        sai_object_id_t m_otherPort = SAI_NULL_OBJECT_ID;
        sai_object_id_t m_otherBridgePort = SAI_NULL_OBJECT_ID;
};

constexpr uint16_t SwitchVppFdbTest::VLAN_ID;
constexpr const char *SwitchVppFdbTest::PORT_HWIF;
constexpr const char *SwitchVppFdbTest::OTHER_PORT_HWIF;
constexpr const char *SwitchVppFdbTest::MEMBER_HWIF;

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

/*
 * VPP ages the MAC addresses each bridge domain has learned, in whole minutes
 * (mac_age, 0: never), and reports each aged one as a DELETE MAC event. SAI's
 * FDB aging time is in seconds, for the whole switch.
 */
TEST(SwitchVpp, fdbAgingTimeRoundsUpToWholeMinutes)
{
    EXPECT_EQ(0, SwitchVpp::fdb_aging_time_to_mac_age(0));
    EXPECT_EQ(1, SwitchVpp::fdb_aging_time_to_mac_age(1));
    EXPECT_EQ(1, SwitchVpp::fdb_aging_time_to_mac_age(60));
    EXPECT_EQ(2, SwitchVpp::fdb_aging_time_to_mac_age(61));
    EXPECT_EQ(10, SwitchVpp::fdb_aging_time_to_mac_age(600));
    EXPECT_EQ(255, SwitchVpp::fdb_aging_time_to_mac_age(255 * 60));
    EXPECT_EQ(255, SwitchVpp::fdb_aging_time_to_mac_age(255 * 60 + 1));
    EXPECT_EQ(255, SwitchVpp::fdb_aging_time_to_mac_age(UINT32_MAX));
}

/*
 * The fixture's switch, VLAN 100 and ports, with the switch object orchagent
 * sets SAI_SWITCH_ATTR_FDB_AGING_TIME on.
 */
class SwitchVppFdbAging : public SwitchVppFdbTest
{
    protected:

        void SetUp() override
        {
            SwitchVppFdbTest::SetUp();

            // vslib's default
            sai_attribute_t attr;

            attr.id = SAI_SWITCH_ATTR_FDB_AGING_TIME;
            attr.value.u32 = 0;

            ASSERT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create_internal(SAI_OBJECT_TYPE_SWITCH, sai_serialize_object_id(m_switchId), m_switchId, 1, &attr));
        }

        // orchagent sets SAI_SWITCH_ATTR_FDB_AGING_TIME
        sai_status_t setAgingTime(
                uint32_t aging_time)
        {
            SWSS_LOG_ENTER();

            sai_attribute_t attr;

            attr.id = SAI_SWITCH_ATTR_FDB_AGING_TIME;
            attr.value.u32 = aging_time;

            return m_sw->set(SAI_OBJECT_TYPE_SWITCH, sai_serialize_object_id(m_switchId), &attr);
        }

        uint32_t agingTime()
        {
            SWSS_LOG_ENTER();

            sai_attribute_t attr;

            attr.id = SAI_SWITCH_ATTR_FDB_AGING_TIME;

            EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->get(SAI_OBJECT_TYPE_SWITCH, m_switchId, 1, &attr));

            return attr.value.u32;
        }

        // the mac_age values set on bridge domain bd_id, in order
        std::vector<uint32_t> macAges(
                uint32_t bd_id)
        {
            SWSS_LOG_ENTER();

            std::vector<uint32_t> ages;

            for (auto& call: vppMock().callsTo("vpp_bridge_domain_set_mac_age", std::to_string(bd_id)))
            {
                ages.push_back(call.sub_id);
            }

            return ages;
        }

        // the mac_age VPP has on bridge domain bd_id
        uint32_t macAge(
                uint32_t bd_id)
        {
            SWSS_LOG_ENTER();

            auto bd = vppMock().bdMacAges.find(bd_id);

            return bd == vppMock().bdMacAges.end() ? 0 : bd->second;
        }

        // orchagent creates another VLAN
        sai_object_id_t addVlan(
                uint16_t vlan_id)
        {
            SWSS_LOG_ENTER();

            auto vlan = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_VLAN, m_switchId);

            sai_attribute_t attr;

            attr.id = SAI_VLAN_ATTR_VLAN_ID;
            attr.value.u16 = vlan_id;

            EXPECT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create_internal(SAI_OBJECT_TYPE_VLAN, sai_serialize_object_id(vlan), m_switchId, 1, &attr));

            return vlan;
        }

        // VLAN 300, with a member on the other port, and VLAN 100 with one on
        // the first port: bridge domains 300 and 100
        void addTwoVlans()
        {
            SWSS_LOG_ENTER();

            addTaggedMember(m_bridgePort);
            addTaggedMember(m_otherBridgePort, addVlan(OTHER_VLAN_ID));
        }

        // orchagent creates VLAN 100's router interface: its bridge virtual
        // interface joins bridge domain 100, which VPP creates if no member did
        void addVlanRouterInterface()
        {
            SWSS_LOG_ENTER();

            auto vrf = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, m_switchId);

            sai_attribute_t attr{};

            attr.id = SAI_SWITCH_ATTR_DEFAULT_VIRTUAL_ROUTER_ID;
            attr.value.oid = vrf;

            ASSERT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->set_internal(SAI_OBJECT_TYPE_SWITCH, sai_serialize_object_id(m_switchId), &attr));

            sai_attribute_t attrs[3]{};

            attrs[0].id = SAI_ROUTER_INTERFACE_ATTR_VLAN_ID;
            attrs[0].value.oid = m_vlan;
            attrs[1].id = SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID;
            attrs[1].value.oid = vrf;
            attrs[2].id = SAI_ROUTER_INTERFACE_ATTR_SRC_MAC_ADDRESS;

            SwitchVpp::RifCreateState state;

            ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->vpp_create_bvi_interface(3, attrs, state));
            ASSERT_EQ(std::set<std::string>({"bvi100"}), vppMock().bdMembers[VLAN_ID]);
        }

        static constexpr uint16_t OTHER_VLAN_ID = 300;
};

constexpr uint16_t SwitchVppFdbAging::OTHER_VLAN_ID;

TEST_F(SwitchVppFdbAging, AgingTimeAppliesToTheBridgeDomainsOfVlansWithMembers)
{
    // Members join bridge domain 100 with the switch's aging time, 0: no aging
    addTaggedMember(m_bridgePort);
    addTaggedMember(m_otherBridgePort);

    EXPECT_EQ(0u, macAge(VLAN_ID));

    vppMock().calls.clear();

    // Once per bridge domain, however many members it has
    ASSERT_EQ(SAI_STATUS_SUCCESS, setAgingTime(600));

    EXPECT_EQ(std::vector<uint32_t>({10}), macAges(VLAN_ID));
    EXPECT_EQ(600u, agingTime());

    // An aging time with the same MAC age changes nothing in VPP
    ASSERT_EQ(SAI_STATUS_SUCCESS, setAgingTime(590));

    EXPECT_EQ(std::vector<uint32_t>({10}), macAges(VLAN_ID));
    EXPECT_EQ(590u, agingTime());

    // 0 turns aging off
    ASSERT_EQ(SAI_STATUS_SUCCESS, setAgingTime(0));

    EXPECT_EQ(std::vector<uint32_t>({10, 0}), macAges(VLAN_ID));
    EXPECT_EQ(0u, agingTime());
}

TEST_F(SwitchVppFdbAging, BridgeDomainCreatedAfterTheAgingTimeIsSetAges)
{
    // No VLAN has a member yet, so VPP has no bridge domain to set
    ASSERT_EQ(SAI_STATUS_SUCCESS, setAgingTime(90));

    EXPECT_TRUE(vppMock().callsTo("vpp_bridge_domain_set_mac_age", std::to_string(VLAN_ID)).empty());

    // The first member makes VPP create bridge domain 100, which then ages
    addTaggedMember(m_bridgePort);

    EXPECT_EQ(std::vector<uint32_t>({2}), macAges(VLAN_ID));
}

/*
 * VPP keeps the bridge domain of a VLAN with a router interface when the
 * VLAN's last member leaves, as the VLAN's bridge virtual interface is still
 * in it, and the bridge domain keeps its MAC age. An aging time set while the
 * VLAN has no member doesn't reach it: the next member sets it, also when it
 * turns aging off.
 */
TEST_F(SwitchVppFdbAging, BridgeDomainKeptForItsBviGetsTheAgingTimeWhenAMemberJoins)
{
    addVlanRouterInterface();

    ASSERT_EQ(SAI_STATUS_SUCCESS, setAgingTime(600));

    auto member = addTaggedMember(m_bridgePort);

    EXPECT_EQ(10u, macAge(VLAN_ID));

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->removeVlanMember(member));
    ASSERT_EQ(std::set<std::string>({"bvi100"}), vppMock().bdMembers[VLAN_ID]);

    ASSERT_EQ(SAI_STATUS_SUCCESS, setAgingTime(0));

    EXPECT_EQ(10u, macAge(VLAN_ID));

    addTaggedMember(m_bridgePort);

    EXPECT_EQ(0u, macAge(VLAN_ID));
}

/*
 * VPP creates the bridge domain of a VLAN for its router interface's bridge
 * virtual interface if no member did. That interface is no member: the first
 * member sets the switch's aging time, set before the router interface was
 * created.
 */
TEST_F(SwitchVppFdbAging, FirstMemberOfAVlanWithARouterInterfaceSetsTheAgingTime)
{
    ASSERT_EQ(SAI_STATUS_SUCCESS, setAgingTime(600));

    addVlanRouterInterface();

    EXPECT_EQ(0u, macAge(VLAN_ID));

    addTaggedMember(m_bridgePort);

    EXPECT_EQ(10u, macAge(VLAN_ID));
}

/*
 * VPP fails to set the MAC age of one of two bridge domains: the set fails,
 * the aging time is not stored, and setting it again sets both.
 */
TEST_F(SwitchVppFdbAging, AgingTimeVppFailsToSetIsNotStored)
{
    addTwoVlans();

    ASSERT_EQ(SAI_STATUS_SUCCESS, setAgingTime(600));

    vppMock().macAgeFailures.insert(OTHER_VLAN_ID);

    EXPECT_EQ(SAI_STATUS_FAILURE, setAgingTime(0));

    EXPECT_EQ(600u, agingTime());
    EXPECT_EQ(0u, macAge(VLAN_ID));
    EXPECT_EQ(10u, macAge(OTHER_VLAN_ID));

    vppMock().macAgeFailures.clear();

    EXPECT_EQ(SAI_STATUS_SUCCESS, setAgingTime(0));

    EXPECT_EQ(0u, agingTime());
    EXPECT_EQ(0u, macAge(VLAN_ID));
    EXPECT_EQ(0u, macAge(OTHER_VLAN_ID));
}

/*
 * After a failed set, a bridge domain may not have the stored aging time
 * anymore: setting the stored aging time again sets it on every bridge domain.
 */
TEST_F(SwitchVppFdbAging, StoredAgingTimeIsSetAgainAfterAFailure)
{
    addTwoVlans();

    ASSERT_EQ(SAI_STATUS_SUCCESS, setAgingTime(600));

    vppMock().macAgeFailures.insert(OTHER_VLAN_ID);

    ASSERT_EQ(SAI_STATUS_FAILURE, setAgingTime(0));
    ASSERT_EQ(0u, macAge(VLAN_ID));

    vppMock().macAgeFailures.clear();

    EXPECT_EQ(SAI_STATUS_SUCCESS, setAgingTime(600));

    EXPECT_EQ(600u, agingTime());
    EXPECT_EQ(10u, macAge(VLAN_ID));
    EXPECT_EQ(10u, macAge(OTHER_VLAN_ID));
}

/*
 * VPP fails to set the MAC age of the bridge domain a member joins: the
 * member still comes up, and the next aging time set reaches the bridge
 * domain, even one with the same MAC age.
 */
TEST_F(SwitchVppFdbAging, BridgeDomainVppFailsToSetForAMemberGetsTheNextAgingTime)
{
    ASSERT_EQ(SAI_STATUS_SUCCESS, setAgingTime(600));

    vppMock().macAgeFailures.insert(VLAN_ID);

    addTaggedMember(m_bridgePort);

    EXPECT_EQ(std::set<std::string>({MEMBER_HWIF}), vppMock().bdMembers[VLAN_ID]);
    EXPECT_EQ(0u, macAge(VLAN_ID));

    vppMock().macAgeFailures.clear();

    EXPECT_EQ(SAI_STATUS_SUCCESS, setAgingTime(600));

    EXPECT_EQ(10u, macAge(VLAN_ID));
}

/*
 * The bridge domain of a VLAN with a member has the switch's MAC age: a
 * config reload adds every server port to the VLAN, and only the first one
 * sets it.
 */
TEST_F(SwitchVppFdbAging, MemberJoiningABridgeDomainWithAMemberSetsNothing)
{
    ASSERT_EQ(SAI_STATUS_SUCCESS, setAgingTime(600));

    addTaggedMember(m_bridgePort);
    addTaggedMember(m_otherBridgePort);

    EXPECT_EQ(std::vector<uint32_t>({10}), macAges(VLAN_ID));
}

/*
 * After VPP failed to set a MAC age, a bridge domain with a member may not
 * have the stored aging time: a member joining it sets it.
 */
TEST_F(SwitchVppFdbAging, MemberJoiningAfterAFailureSetsTheStoredAgingTime)
{
    addTwoVlans();

    ASSERT_EQ(SAI_STATUS_SUCCESS, setAgingTime(600));

    // Turning aging off fails on bridge domain 300 only: 600 s stays stored
    vppMock().macAgeFailures.insert(OTHER_VLAN_ID);

    ASSERT_EQ(SAI_STATUS_FAILURE, setAgingTime(0));
    ASSERT_EQ(0u, macAge(VLAN_ID));

    // The other port joins VLAN 100 too
    addTaggedMember(m_otherBridgePort);

    EXPECT_EQ(10u, macAge(VLAN_ID));
}

/*
 * Creating the switch, vslib makes the bridge ports members of the default
 * VLAN, VLAN 1, only as SAI objects: SwitchStateBase::create_vlan_members()
 * doesn't go through SwitchVpp, so VPP has no bridge domain 1, and rejects a
 * MAC age for it. orchagent removes these members, but syncd's cold replay
 * restores the switch attributes before that.
 */
TEST_F(SwitchVppFdbAging, AgingTimeIgnoresDefaultVlanMembersVppDoesNotHave)
{
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->create_default_vlan());

    m_sw->m_bridge_port_list_port_based = {m_bridgePort, m_otherBridgePort};

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->create_vlan_members());
    ASSERT_EQ(2u, m_sw->m_objectHash.at(SAI_OBJECT_TYPE_VLAN_MEMBER).size());
    ASSERT_TRUE(vppMock().bdMembers.empty());

    EXPECT_EQ(SAI_STATUS_SUCCESS, setAgingTime(600));

    EXPECT_EQ(600u, agingTime());
    EXPECT_TRUE(vppMock().callsTo("vpp_bridge_domain_set_mac_age", "1").empty());

    // A member orchagent creates reaches VPP, and its bridge domain ages
    addTaggedMember(m_bridgePort);

    EXPECT_EQ(10u, macAge(VLAN_ID));
}

/*
 * VPP never ages an FDB entry orchagent creates, as it adds it over its API,
 * until traffic from the MAC on another port moves it: VPP then reports the
 * move, and ages the entry like a learned MAC.
 */
class SwitchVppFdbAgingCreatedEntry : public SwitchVppFdbAging
{
    protected:

        void SetUp() override
        {
            SwitchVppFdbAging::SetUp();

            sai_attribute_t attr;

            attr.id = SAI_SWITCH_ATTR_FDB_EVENT_NOTIFY;
            attr.value.ptr = reinterpret_cast<void *>(0x1);

            ASSERT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->set_internal(SAI_OBJECT_TYPE_SWITCH, sai_serialize_object_id(m_switchId), &attr));

            // Both ports are untagged members of VLAN 100, in bridge domain 100
            addUntaggedMember(PORT_HWIF, PORT_SW_IF_INDEX, m_port);
            addUntaggedMember(OTHER_PORT_HWIF, OTHER_PORT_SW_IF_INDEX, m_otherPort);

            createEntry();
        }

        // orchagent creates its dynamic FDB entry, on the first port
        void createEntry()
        {
            SWSS_LOG_ENTER();

            sai_fdb_entry_t fdb_entry = {};

            fdb_entry.switch_id = m_switchId;
            fdb_entry.bv_id = m_vlan;
            memcpy(fdb_entry.mac_address, MAC, sizeof(sai_mac_t));

            m_fdbId = sai_serialize_fdb_entry(fdb_entry);

            sai_attribute_t attrs[2];

            attrs[0].id = SAI_FDB_ENTRY_ATTR_TYPE;
            attrs[0].value.s32 = SAI_FDB_ENTRY_TYPE_DYNAMIC;
            attrs[1].id = SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID;
            attrs[1].value.oid = m_bridgePort;

            ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->create(SAI_OBJECT_TYPE_FDB_ENTRY, m_fdbId, m_switchId, 2, attrs));
            ASSERT_FALSE(vppMock().callsTo("l2fib_add_del", std::string(PORT_HWIF) + " " + MAC_STR).empty());
        }

        void addUntaggedMember(
                const char *hwif,
                uint32_t sw_if_index,
                sai_object_id_t port)
        {
            SWSS_LOG_ENTER();

            ASSERT_TRUE(m_sw->m_ifaceRegistry.bindSwIfIndex(hwif, sw_if_index));
            ASSERT_TRUE(m_sw->m_ifaceRegistry.setBdId(hwif, VLAN_ID));

            sai_attribute_t attr;

            attr.id = SAI_PORT_ATTR_PORT_VLAN_ID;
            attr.value.u16 = VLAN_ID;

            ASSERT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->set_internal(SAI_OBJECT_TYPE_PORT, sai_serialize_object_id(port), &attr));
        }

        // VPP reports the MAC, or another one, on sw_if_index
        void queueMacEvent(
                uint8_t action,
                uint32_t sw_if_index,
                const uint8_t *mac = MAC)
        {
            SWSS_LOG_ENTER();

            SwitchVpp::VppMacEvent ev = {};

            memcpy(ev.mac, mac, sizeof(ev.mac));
            ev.sw_if_index = sw_if_index;
            ev.action = action;

            m_sw->m_mac_event_queue.push(ev);
        }

        // VPP reports the MAC, or another one, on sw_if_index, and saivpp's aging pass takes it
        void macEvent(
                uint8_t action,
                uint32_t sw_if_index,
                const uint8_t *mac = MAC)
        {
            SWSS_LOG_ENTER();

            queueMacEvent(action, sw_if_index, mac);
            m_sw->processFdbEntriesForAging();
        }

        // VPP's L2FIB has the MAC, or another one, on sw_if_index
        void l2fibHasMac(
                uint32_t sw_if_index,
                const uint8_t *mac = MAC)
        {
            SWSS_LOG_ENTER();

            vpp_l2fib_entry_t row = {};

            memcpy(row.mac, mac, sizeof(row.mac));
            row.bd_id = VLAN_ID;
            row.sw_if_index = sw_if_index;

            vppMock().l2fib = {row};
        }

        // orchagent removes the port from VLAN 100, its only VLAN, and so its bridge port
        void leaveVlan(
                const char *hwif,
                sai_object_id_t bridgePort)
        {
            SWSS_LOG_ENTER();

            ASSERT_TRUE(m_sw->m_ifaceRegistry.clearBdId(hwif));
            ASSERT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->remove_internal(SAI_OBJECT_TYPE_BRIDGE_PORT, sai_serialize_object_id(bridgePort)));
        }

        // the event socket reconnected, and VPP's L2FIB has the MAC on sw_if_index
        void resync(
                uint32_t sw_if_index)
        {
            SWSS_LOG_ENTER();

            l2fibHasMac(sw_if_index);

            auto dumps = vppMock().l2fibDumpCalls;

            m_sw->m_mac_event_queue.push({{0}, ~0u, VPP_MAC_ACTION_RESYNC});
            m_sw->processFdbEntriesForAging();

            EXPECT_EQ(dumps + 1, vppMock().l2fibDumpCalls);
        }

        // the one FDB notification saivpp sent since the last call
        std::string fdbNotification()
        {
            SWSS_LOG_ENTER();

            auto queue = m_sw->m_switchConfig->m_eventQueue;

            EXPECT_EQ(1u, queue->size());

            if (queue->size() == 0)
            {
                return "";
            }

            auto event = queue->dequeue();

            EXPECT_EQ(EVENT_TYPE_NOTIFICATION, event->getType());

            auto ntf = std::dynamic_pointer_cast<EventPayloadNotification>(event->getPayload())->getNotification();

            EXPECT_EQ(SAI_SWITCH_NOTIFICATION_TYPE_FDB_EVENT, ntf->getNotificationType());

            return ntf->getSerializedNotification();
        }

        sai_object_id_t entryBridgePort()
        {
            SWSS_LOG_ENTER();

            sai_attribute_t attr;

            attr.id = SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID;

            EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->get(SAI_OBJECT_TYPE_FDB_ENTRY, m_fdbId, 1, &attr));

            return attr.value.oid;
        }

        static constexpr uint8_t MAC[6] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x66};
        static constexpr const char *MAC_STR = "02:11:22:33:44:66";
        // a MAC with no FDB entry
        static constexpr uint8_t OTHER_MAC[6] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x77};
        static constexpr const char *OTHER_MAC_STR = "02:11:22:33:44:77";
        static constexpr uint32_t PORT_SW_IF_INDEX = 5;
        static constexpr uint32_t OTHER_PORT_SW_IF_INDEX = 6;

        std::string m_fdbId;
};

constexpr uint8_t SwitchVppFdbAgingCreatedEntry::MAC[6];
constexpr const char *SwitchVppFdbAgingCreatedEntry::MAC_STR;
constexpr uint8_t SwitchVppFdbAgingCreatedEntry::OTHER_MAC[6];
constexpr const char *SwitchVppFdbAgingCreatedEntry::OTHER_MAC_STR;
constexpr uint32_t SwitchVppFdbAgingCreatedEntry::PORT_SW_IF_INDEX;
constexpr uint32_t SwitchVppFdbAgingCreatedEntry::OTHER_PORT_SW_IF_INDEX;

TEST_F(SwitchVppFdbAgingCreatedEntry, EntryVppMovesIsReportedAndAgesOut)
{
    // Traffic from the MAC on the other port: VPP moves the entry
    l2fibHasMac(OTHER_PORT_SW_IF_INDEX);
    macEvent(VPP_MAC_ACTION_MOVE, OTHER_PORT_SW_IF_INDEX);

    auto ntf = fdbNotification();

    EXPECT_NE(std::string::npos, ntf.find("SAI_FDB_EVENT_MOVE"));
    EXPECT_NE(std::string::npos, ntf.find(MAC_STR));
    EXPECT_EQ(m_otherBridgePort, entryBridgePort());

    // From then on VPP ages it like a learned MAC, and its aging is reported
    macEvent(VPP_MAC_ACTION_DELETE, OTHER_PORT_SW_IF_INDEX);

    ntf = fdbNotification();

    EXPECT_NE(std::string::npos, ntf.find("SAI_FDB_EVENT_AGED"));
    EXPECT_NE(std::string::npos, ntf.find(MAC_STR));
    EXPECT_EQ(0u, m_sw->m_objectHash.at(SAI_OBJECT_TYPE_FDB_ENTRY).count(m_fdbId));
    EXPECT_TRUE(m_sw->m_fdb_info_set.empty());
    EXPECT_TRUE(m_sw->m_vpp_fdb_entries.empty());
}

TEST_F(SwitchVppFdbAgingCreatedEntry, EntryRemovedAfterVppMovedItIsForgotten)
{
    l2fibHasMac(OTHER_PORT_SW_IF_INDEX);
    macEvent(VPP_MAC_ACTION_MOVE, OTHER_PORT_SW_IF_INDEX);

    EXPECT_NE(std::string::npos, fdbNotification().find("SAI_FDB_EVENT_MOVE"));

    // orchagent removes the entry: VPP deletes it where it moved it to, and
    // saivpp forgets the MAC
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->remove(SAI_OBJECT_TYPE_FDB_ENTRY, m_fdbId));

    auto dels = vppMock().callsTo("l2fib_add_del", std::string(OTHER_PORT_HWIF) + " " + MAC_STR);

    ASSERT_EQ(1u, dels.size());
    EXPECT_EQ(VLAN_ID, dels[0].id);
    EXPECT_FALSE(dels[0].flag);
    EXPECT_TRUE(m_sw->m_fdb_info_set.empty());
    EXPECT_TRUE(m_sw->m_vpp_fdb_entries.empty());

    // VPP learns the MAC again: reported as learned
    macEvent(VPP_MAC_ACTION_ADD, OTHER_PORT_SW_IF_INDEX);

    EXPECT_NE(std::string::npos, fdbNotification().find("SAI_FDB_EVENT_LEARNED"));
}

/*
 * VPP moved the entry while its MOVE event was lost: the resync takes the move.
 */
TEST_F(SwitchVppFdbAgingCreatedEntry, EntryVppMovedWhileItsEventsWereLostIsMovedByTheResync)
{
    resync(OTHER_PORT_SW_IF_INDEX);

    auto ntf = fdbNotification();

    EXPECT_NE(std::string::npos, ntf.find("SAI_FDB_EVENT_MOVE"));
    EXPECT_NE(std::string::npos, ntf.find(MAC_STR));
    EXPECT_EQ(m_otherBridgePort, entryBridgePort());

    // and its aging is reported
    macEvent(VPP_MAC_ACTION_DELETE, OTHER_PORT_SW_IF_INDEX);

    EXPECT_NE(std::string::npos, fdbNotification().find("SAI_FDB_EVENT_AGED"));
    EXPECT_EQ(0u, m_sw->m_objectHash.at(SAI_OBJECT_TYPE_FDB_ENTRY).count(m_fdbId));
    EXPECT_TRUE(m_sw->m_vpp_fdb_entries.empty());
}

/*
 * VPP has the entry on its own port: not a MAC VPP learned, nothing to report.
 */
TEST_F(SwitchVppFdbAgingCreatedEntry, EntryOnItsPortIsLeftAloneByTheResync)
{
    resync(PORT_SW_IF_INDEX);

    EXPECT_EQ(0u, m_sw->m_switchConfig->m_eventQueue->size());
    EXPECT_EQ(m_bridgePort, entryBridgePort());
    EXPECT_TRUE(m_sw->m_fdb_info_set.empty());
    EXPECT_TRUE(m_sw->m_vpp_fdb_entries.empty());
}

/*
 * VPP moved the entry away and back while its events were lost: the resync
 * finds it on its own port, but VPP now ages it, and its aging is reported.
 */
TEST_F(SwitchVppFdbAgingCreatedEntry, EntryVppMovedAwayAndBackWhileItsEventsWereLostAgesOut)
{
    resync(PORT_SW_IF_INDEX);

    EXPECT_EQ(0u, m_sw->m_switchConfig->m_eventQueue->size());

    // VPP ages it: out of its L2FIB, then the DELETE
    vppMock().l2fib.clear();
    macEvent(VPP_MAC_ACTION_DELETE, PORT_SW_IF_INDEX);

    auto ntf = fdbNotification();

    EXPECT_NE(std::string::npos, ntf.find("SAI_FDB_EVENT_AGED"));
    EXPECT_NE(std::string::npos, ntf.find(MAC_STR));
    EXPECT_EQ(0u, m_sw->m_objectHash.at(SAI_OBJECT_TYPE_FDB_ENTRY).count(m_fdbId));
    EXPECT_TRUE(m_sw->m_fdb_info_set.empty());
    EXPECT_TRUE(m_sw->m_vpp_fdb_entries.empty());
}

/*
 * VPP doesn't age a static entry, and a DELETE doesn't remove one.
 */
TEST_F(SwitchVppFdbAgingCreatedEntry, StaticEntryIsNotRemovedByADelete)
{
    sai_attribute_t attr;

    attr.id = SAI_FDB_ENTRY_ATTR_TYPE;
    attr.value.s32 = SAI_FDB_ENTRY_TYPE_STATIC;

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->set_internal(SAI_OBJECT_TYPE_FDB_ENTRY, m_fdbId, &attr));

    macEvent(VPP_MAC_ACTION_DELETE, PORT_SW_IF_INDEX);

    EXPECT_EQ(0u, m_sw->m_switchConfig->m_eventQueue->size());
    EXPECT_EQ(1u, m_sw->m_objectHash.at(SAI_OBJECT_TYPE_FDB_ENTRY).count(m_fdbId));
    EXPECT_TRUE(m_sw->m_fdb_info_set.empty());
}

/*
 * A MAC VPP learned, which orchagent then makes static: set() changes the
 * entry's type in SAI only, and VPP keeps its learned row, which ages. Its
 * DELETE is reported once, as AGED with the entry's bridge port, and the
 * entry is removed and forgotten, so orchagent can create the static entry
 * again.
 */
TEST_F(SwitchVppFdbAgingCreatedEntry, LearnedEntryMadeStaticIsAgedWhenVppDeletesIt)
{
    l2fibHasMac(PORT_SW_IF_INDEX, OTHER_MAC);
    macEvent(VPP_MAC_ACTION_ADD, PORT_SW_IF_INDEX, OTHER_MAC);

    EXPECT_NE(std::string::npos, fdbNotification().find("SAI_FDB_EVENT_LEARNED"));
    ASSERT_EQ(1u, m_sw->m_vpp_fdb_entries.size());

    sai_fdb_entry_t fdb_entry = {};

    fdb_entry.switch_id = m_switchId;
    fdb_entry.bv_id = m_vlan;
    memcpy(fdb_entry.mac_address, OTHER_MAC, sizeof(sai_mac_t));

    auto learnedId = sai_serialize_fdb_entry(fdb_entry);

    ASSERT_EQ(1u, m_sw->m_objectHash.at(SAI_OBJECT_TYPE_FDB_ENTRY).count(learnedId));

    sai_attribute_t attr;

    attr.id = SAI_FDB_ENTRY_ATTR_TYPE;
    attr.value.s32 = SAI_FDB_ENTRY_TYPE_STATIC;

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->set(SAI_OBJECT_TYPE_FDB_ENTRY, learnedId, &attr));

    // VPP ages its learned row
    vppMock().l2fib.clear();
    macEvent(VPP_MAC_ACTION_DELETE, PORT_SW_IF_INDEX, OTHER_MAC);

    auto ntf = fdbNotification();

    EXPECT_NE(std::string::npos, ntf.find("SAI_FDB_EVENT_AGED"));
    EXPECT_NE(std::string::npos, ntf.find(OTHER_MAC_STR));
    EXPECT_NE(std::string::npos, ntf.find(sai_serialize_object_id(m_bridgePort)));
    EXPECT_EQ(0u, m_sw->m_objectHash.at(SAI_OBJECT_TYPE_FDB_ENTRY).count(learnedId));
    EXPECT_TRUE(m_sw->m_vpp_fdb_entries.empty());
    EXPECT_TRUE(m_sw->m_fdb_info_set.empty());

    // orchagent's entry for the other MAC stays
    EXPECT_EQ(1u, m_sw->m_objectHash.at(SAI_OBJECT_TYPE_FDB_ENTRY).count(m_fdbId));
}

/*
 * VPP's DELETE waited in the queue while orchagent removed the entry and
 * created it again: VPP has the new entry, and nothing is aged.
 */
TEST_F(SwitchVppFdbAgingCreatedEntry, DeleteQueuedBeforeOrchagentRecreatedTheEntryAgesNothing)
{
    queueMacEvent(VPP_MAC_ACTION_DELETE, PORT_SW_IF_INDEX);

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->remove(SAI_OBJECT_TYPE_FDB_ENTRY, m_fdbId));
    createEntry();
    l2fibHasMac(PORT_SW_IF_INDEX);

    m_sw->processFdbEntriesForAging();

    EXPECT_EQ(1u, vppMock().l2fibDumpCalls);
    EXPECT_EQ(0u, m_sw->m_switchConfig->m_eventQueue->size());
    EXPECT_EQ(1u, m_sw->m_objectHash.at(SAI_OBJECT_TYPE_FDB_ENTRY).count(m_fdbId));
    EXPECT_TRUE(m_sw->m_fdb_info_set.empty());
}

/*
 * VPP's L2FIB can't be dumped to check the DELETE: the next aging pass retries it.
 */
TEST_F(SwitchVppFdbAgingCreatedEntry, DeleteRetriedAfterAFailedL2fibDump)
{
    vppMock().l2fibDumpResult = -1;

    macEvent(VPP_MAC_ACTION_DELETE, PORT_SW_IF_INDEX);

    EXPECT_EQ(0u, m_sw->m_switchConfig->m_eventQueue->size());
    EXPECT_EQ(1u, m_sw->m_objectHash.at(SAI_OBJECT_TYPE_FDB_ENTRY).count(m_fdbId));
    ASSERT_EQ(1u, m_sw->m_mac_event_queue.size());
    EXPECT_EQ(static_cast<uint8_t>(VPP_MAC_ACTION_DELETE), m_sw->m_mac_event_queue.front().action);
    EXPECT_TRUE(m_sw->m_mac_event_queue.front().retry);

    // The dump works again, and VPP doesn't have the MAC: its aging is reported
    vppMock().l2fibDumpResult = 0;

    m_sw->processFdbEntriesForAging();

    EXPECT_EQ(2u, vppMock().l2fibDumpCalls);
    EXPECT_TRUE(m_sw->m_mac_event_queue.empty());
    EXPECT_NE(std::string::npos, fdbNotification().find("SAI_FDB_EVENT_AGED"));
    EXPECT_EQ(0u, m_sw->m_objectHash.at(SAI_OBJECT_TYPE_FDB_ENTRY).count(m_fdbId));
    EXPECT_TRUE(m_sw->m_fdb_info_set.empty());
}

/*
 * VPP moved orchagent's entry to the other port while its events were lost,
 * and ages it there. VPP's L2FIB can't be dumped to check the DELETE, and the
 * other port leaves the VLAN before the retry: the retry still takes the
 * DELETE in bridge domain 100, and ages the entry on the first port.
 */
TEST_F(SwitchVppFdbAgingCreatedEntry, DeleteRetriedAfterItsPortLeftTheVlanAgesTheEntry)
{
    vppMock().l2fibDumpFailures = 1;

    macEvent(VPP_MAC_ACTION_DELETE, OTHER_PORT_SW_IF_INDEX);

    EXPECT_EQ(0u, m_sw->m_switchConfig->m_eventQueue->size());
    ASSERT_EQ(1u, m_sw->m_mac_event_queue.size());
    EXPECT_TRUE(m_sw->m_mac_event_queue.front().retry);

    leaveVlan(OTHER_PORT_HWIF, m_otherBridgePort);

    m_sw->processFdbEntriesForAging();

    EXPECT_EQ(2u, vppMock().l2fibDumpCalls);
    EXPECT_TRUE(m_sw->m_mac_event_queue.empty());

    auto ntf = fdbNotification();

    EXPECT_NE(std::string::npos, ntf.find("SAI_FDB_EVENT_AGED"));
    EXPECT_NE(std::string::npos, ntf.find(MAC_STR));
    EXPECT_NE(std::string::npos, ntf.find(sai_serialize_object_id(m_bridgePort)));
    EXPECT_EQ(0u, m_sw->m_objectHash.at(SAI_OBJECT_TYPE_FDB_ENTRY).count(m_fdbId));
    EXPECT_TRUE(m_sw->m_fdb_info_set.empty());
    EXPECT_TRUE(m_sw->m_vpp_fdb_entries.empty());
}

/*
 * The same, but by the retry VPP has the MAC again, on the first port. The
 * retry looks the MAC up in bridge domain 100, and ages nothing.
 */
TEST_F(SwitchVppFdbAgingCreatedEntry, DeleteRetriedAfterItsPortLeftTheVlanAgesNothingVppHas)
{
    vppMock().l2fibDumpFailures = 1;

    macEvent(VPP_MAC_ACTION_DELETE, OTHER_PORT_SW_IF_INDEX);

    ASSERT_EQ(1u, m_sw->m_mac_event_queue.size());

    leaveVlan(OTHER_PORT_HWIF, m_otherBridgePort);
    l2fibHasMac(PORT_SW_IF_INDEX);

    m_sw->processFdbEntriesForAging();

    EXPECT_EQ(2u, vppMock().l2fibDumpCalls);
    EXPECT_TRUE(m_sw->m_mac_event_queue.empty());
    EXPECT_EQ(0u, m_sw->m_switchConfig->m_eventQueue->size());
    EXPECT_EQ(1u, m_sw->m_objectHash.at(SAI_OBJECT_TYPE_FDB_ENTRY).count(m_fdbId));
    EXPECT_EQ(m_bridgePort, entryBridgePort());
    EXPECT_TRUE(m_sw->m_fdb_info_set.empty());
    EXPECT_TRUE(m_sw->m_vpp_fdb_entries.empty());
}

/*
 * A DELETE queued again waits behind the events VPP sent later: by the retry,
 * VPP may have the MAC again, as here where it moved it, and the MOVE made
 * saivpp track the entry. The retry checks VPP's L2FIB all the same.
 */
TEST_F(SwitchVppFdbAgingCreatedEntry, DeleteRetriedBehindAMoveAgesNothing)
{
    vppMock().l2fibDumpFailures = 1;

    queueMacEvent(VPP_MAC_ACTION_DELETE, PORT_SW_IF_INDEX);
    queueMacEvent(VPP_MAC_ACTION_MOVE, OTHER_PORT_SW_IF_INDEX);
    l2fibHasMac(OTHER_PORT_SW_IF_INDEX);

    m_sw->processFdbEntriesForAging();

    EXPECT_NE(std::string::npos, fdbNotification().find("SAI_FDB_EVENT_MOVE"));
    ASSERT_EQ(1u, m_sw->m_mac_event_queue.size());

    m_sw->processFdbEntriesForAging();

    EXPECT_EQ(3u, vppMock().l2fibDumpCalls);
    EXPECT_TRUE(m_sw->m_mac_event_queue.empty());
    EXPECT_EQ(0u, m_sw->m_switchConfig->m_eventQueue->size());
    EXPECT_EQ(m_otherBridgePort, entryBridgePort());
    EXPECT_EQ(1u, m_sw->m_vpp_fdb_entries.size());
    EXPECT_EQ(1u, m_sw->m_fdb_info_set.size());
}

/*
 * The same with the move taken by a resync.
 */
TEST_F(SwitchVppFdbAgingCreatedEntry, DeleteRetriedBehindAResyncAgesNothing)
{
    vppMock().l2fibDumpFailures = 1;

    queueMacEvent(VPP_MAC_ACTION_DELETE, PORT_SW_IF_INDEX);
    m_sw->m_mac_event_queue.push({{0}, ~0u, VPP_MAC_ACTION_RESYNC});
    l2fibHasMac(OTHER_PORT_SW_IF_INDEX);

    m_sw->processFdbEntriesForAging();

    EXPECT_NE(std::string::npos, fdbNotification().find("SAI_FDB_EVENT_MOVE"));
    ASSERT_EQ(1u, m_sw->m_mac_event_queue.size());

    m_sw->processFdbEntriesForAging();

    EXPECT_EQ(3u, vppMock().l2fibDumpCalls);
    EXPECT_TRUE(m_sw->m_mac_event_queue.empty());
    EXPECT_EQ(0u, m_sw->m_switchConfig->m_eventQueue->size());
    EXPECT_EQ(m_otherBridgePort, entryBridgePort());
    EXPECT_EQ(1u, m_sw->m_vpp_fdb_entries.size());
}

/*
 * VPP doesn't move a static entry, but a MOVE VPP sent for the MAC before
 * orchagent made the entry static can still be in the queue: it doesn't move
 * orchagent's static entry, nor does a resync.
 */
TEST_F(SwitchVppFdbAgingCreatedEntry, StaticEntryIsNotMoved)
{
    sai_attribute_t attr;

    attr.id = SAI_FDB_ENTRY_ATTR_TYPE;
    attr.value.s32 = SAI_FDB_ENTRY_TYPE_STATIC;

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->set_internal(SAI_OBJECT_TYPE_FDB_ENTRY, m_fdbId, &attr));

    macEvent(VPP_MAC_ACTION_MOVE, OTHER_PORT_SW_IF_INDEX);
    resync(OTHER_PORT_SW_IF_INDEX);

    EXPECT_EQ(0u, m_sw->m_switchConfig->m_eventQueue->size());
    EXPECT_EQ(m_bridgePort, entryBridgePort());
    EXPECT_TRUE(m_sw->m_fdb_info_set.empty());
    EXPECT_TRUE(m_sw->m_vpp_fdb_entries.empty());
}

/*
 * A learn of a MAC SAI has is taken as a move, as of a remote MAC VPP moved
 * to a local port. VPP doesn't move a static entry, but a learn it sent
 * before orchagent's entry was static can still be in the queue: it doesn't
 * move orchagent's static entry either.
 */
TEST_F(SwitchVppFdbAgingCreatedEntry, LearnDoesNotMoveAStaticEntry)
{
    sai_attribute_t attr;

    attr.id = SAI_FDB_ENTRY_ATTR_TYPE;
    attr.value.s32 = SAI_FDB_ENTRY_TYPE_STATIC;

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->set_internal(SAI_OBJECT_TYPE_FDB_ENTRY, m_fdbId, &attr));

    macEvent(VPP_MAC_ACTION_ADD, OTHER_PORT_SW_IF_INDEX);

    EXPECT_EQ(0u, m_sw->m_switchConfig->m_eventQueue->size());
    EXPECT_EQ(m_bridgePort, entryBridgePort());
    EXPECT_TRUE(m_sw->m_fdb_info_set.empty());
    EXPECT_TRUE(m_sw->m_vpp_fdb_entries.empty());
}

/*
 * VPP's MOVE waited in the queue while orchagent removed the entry and
 * created it again: VPP has the new entry where orchagent put it.
 */
TEST_F(SwitchVppFdbAgingCreatedEntry, MoveQueuedBeforeOrchagentRecreatedTheEntryMovesNothing)
{
    queueMacEvent(VPP_MAC_ACTION_MOVE, OTHER_PORT_SW_IF_INDEX);

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->remove(SAI_OBJECT_TYPE_FDB_ENTRY, m_fdbId));
    createEntry();
    l2fibHasMac(PORT_SW_IF_INDEX);

    m_sw->processFdbEntriesForAging();

    EXPECT_EQ(1u, vppMock().l2fibDumpCalls);
    EXPECT_EQ(0u, m_sw->m_switchConfig->m_eventQueue->size());
    EXPECT_EQ(m_bridgePort, entryBridgePort());
    EXPECT_TRUE(m_sw->m_fdb_info_set.empty());
    EXPECT_TRUE(m_sw->m_vpp_fdb_entries.empty());
}

/*
 * VPP's L2FIB can't be dumped to check the MOVE: the next aging pass retries it.
 */
TEST_F(SwitchVppFdbAgingCreatedEntry, MoveRetriedAfterAFailedL2fibDump)
{
    vppMock().l2fibDumpFailures = 1;
    l2fibHasMac(OTHER_PORT_SW_IF_INDEX);

    macEvent(VPP_MAC_ACTION_MOVE, OTHER_PORT_SW_IF_INDEX);

    EXPECT_EQ(0u, m_sw->m_switchConfig->m_eventQueue->size());
    EXPECT_EQ(m_bridgePort, entryBridgePort());
    ASSERT_EQ(1u, m_sw->m_mac_event_queue.size());
    EXPECT_TRUE(m_sw->m_mac_event_queue.front().retry);

    m_sw->processFdbEntriesForAging();

    EXPECT_NE(std::string::npos, fdbNotification().find("SAI_FDB_EVENT_MOVE"));
    EXPECT_EQ(m_otherBridgePort, entryBridgePort());
    EXPECT_EQ(1u, m_sw->m_vpp_fdb_entries.size());
    EXPECT_TRUE(m_sw->m_mac_event_queue.empty());
}

/*
 * A MOVE that waited in the queue while orchagent removed the entry: VPP no
 * longer has the MAC, which is not learned back.
 */
TEST_F(SwitchVppFdbAgingCreatedEntry, MoveQueuedBeforeOrchagentRemovedTheEntryLearnsNothing)
{
    l2fibHasMac(OTHER_PORT_SW_IF_INDEX);
    macEvent(VPP_MAC_ACTION_MOVE, OTHER_PORT_SW_IF_INDEX);

    EXPECT_NE(std::string::npos, fdbNotification().find("SAI_FDB_EVENT_MOVE"));

    // VPP moves it back, and orchagent removes the entry before saivpp takes that
    queueMacEvent(VPP_MAC_ACTION_MOVE, PORT_SW_IF_INDEX);
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->remove(SAI_OBJECT_TYPE_FDB_ENTRY, m_fdbId));
    vppMock().l2fib.clear();

    m_sw->processFdbEntriesForAging();

    EXPECT_EQ(0u, m_sw->m_switchConfig->m_eventQueue->size());
    EXPECT_EQ(0u, m_sw->m_objectHash.at(SAI_OBJECT_TYPE_FDB_ENTRY).count(m_fdbId));
    EXPECT_TRUE(m_sw->m_fdb_info_set.empty());
    EXPECT_TRUE(m_sw->m_vpp_fdb_entries.empty());
}

/*
 * A DELETE retried behind a MOVE that made saivpp track the entry ages it,
 * once VPP no longer has the MAC.
 */
TEST_F(SwitchVppFdbAgingCreatedEntry, DeleteRetriedBehindAMoveAgesTheMacVppDeleted)
{
    vppMock().l2fibDumpFailures = 1;

    queueMacEvent(VPP_MAC_ACTION_DELETE, PORT_SW_IF_INDEX);
    queueMacEvent(VPP_MAC_ACTION_MOVE, OTHER_PORT_SW_IF_INDEX);
    l2fibHasMac(OTHER_PORT_SW_IF_INDEX);

    m_sw->processFdbEntriesForAging();

    EXPECT_NE(std::string::npos, fdbNotification().find("SAI_FDB_EVENT_MOVE"));
    ASSERT_EQ(1u, m_sw->m_mac_event_queue.size());

    // VPP ages it before the retry
    vppMock().l2fib.clear();

    m_sw->processFdbEntriesForAging();

    EXPECT_NE(std::string::npos, fdbNotification().find("SAI_FDB_EVENT_AGED"));
    EXPECT_EQ(0u, m_sw->m_objectHash.at(SAI_OBJECT_TYPE_FDB_ENTRY).count(m_fdbId));
    EXPECT_TRUE(m_sw->m_vpp_fdb_entries.empty());
}

/*
 * A MOVE for a MAC saivpp doesn't track, which VPP has: learned.
 */
TEST_F(SwitchVppFdbAgingCreatedEntry, MoveOfAMacVppHasButSaivppDoesNotTrackIsLearned)
{
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->remove(SAI_OBJECT_TYPE_FDB_ENTRY, m_fdbId));
    l2fibHasMac(OTHER_PORT_SW_IF_INDEX);

    macEvent(VPP_MAC_ACTION_MOVE, OTHER_PORT_SW_IF_INDEX);

    EXPECT_NE(std::string::npos, fdbNotification().find("SAI_FDB_EVENT_LEARNED"));
    EXPECT_EQ(m_otherBridgePort, entryBridgePort());
    EXPECT_EQ(1u, m_sw->m_vpp_fdb_entries.size());
}

/*
 * VPP keeps the row of a port that left the bridge domain until its aging
 * scan deletes it: that row doesn't keep the MAC.
 */
TEST_F(SwitchVppFdbAgingCreatedEntry, RowOfAPortThatLeftTheBridgeDomainDoesNotKeepTheMac)
{
    l2fibHasMac(OTHER_PORT_SW_IF_INDEX);
    ASSERT_TRUE(m_sw->m_ifaceRegistry.clearBdId(OTHER_PORT_HWIF));

    macEvent(VPP_MAC_ACTION_DELETE, PORT_SW_IF_INDEX);

    EXPECT_NE(std::string::npos, fdbNotification().find("SAI_FDB_EVENT_AGED"));
    EXPECT_EQ(0u, m_sw->m_objectHash.at(SAI_OBJECT_TYPE_FDB_ENTRY).count(m_fdbId));
}

/*
 * The same in a resync: that row doesn't move orchagent's entry there.
 */
TEST_F(SwitchVppFdbAgingCreatedEntry, ResyncIgnoresTheRowOfAPortThatLeftTheBridgeDomain)
{
    ASSERT_TRUE(m_sw->m_ifaceRegistry.clearBdId(OTHER_PORT_HWIF));

    resync(OTHER_PORT_SW_IF_INDEX);

    EXPECT_EQ(0u, m_sw->m_switchConfig->m_eventQueue->size());
    EXPECT_EQ(m_bridgePort, entryBridgePort());
    EXPECT_TRUE(m_sw->m_vpp_fdb_entries.empty());
}

/*
 * SAI_BRIDGE_PORT_ATTR_FDB_LEARNING_MODE: VPP's per-interface learn flag, on
 * the VPP interface of each VLAN membership of the bridge port.
 */
class SwitchVppBridgePortLearning : public SwitchVppFdbTest
{
    protected:

        void SetUp() override
        {
            SwitchVppFdbTest::SetUp();

            m_otherVlan = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_VLAN, m_switchId);

            sai_attribute_t vattr;

            vattr.id = SAI_VLAN_ATTR_VLAN_ID;
            vattr.value.u16 = OTHER_VLAN_ID;

            ASSERT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create_internal(SAI_OBJECT_TYPE_VLAN, sai_serialize_object_id(m_otherVlan), m_switchId, 1, &vattr));
        }

        sai_status_t setLearningMode(
                sai_object_id_t bridgePort,
                sai_bridge_port_fdb_learning_mode_t mode)
        {
            SWSS_LOG_ENTER();

            sai_attribute_t attr;

            attr.id = SAI_BRIDGE_PORT_ATTR_FDB_LEARNING_MODE;
            attr.value.s32 = mode;

            return m_sw->set(SAI_OBJECT_TYPE_BRIDGE_PORT, sai_serialize_object_id(bridgePort), &attr);
        }

        // orchagent adds the port to VLAN 200 as an untagged member
        void addUntaggedMember(
                sai_object_id_t bridgePort)
        {
            SWSS_LOG_ENTER();

            auto member = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_VLAN_MEMBER, m_switchId);

            sai_attribute_t attrs[3];

            attrs[0].id = SAI_VLAN_MEMBER_ATTR_VLAN_ID;
            attrs[0].value.oid = m_otherVlan;
            attrs[1].id = SAI_VLAN_MEMBER_ATTR_BRIDGE_PORT_ID;
            attrs[1].value.oid = bridgePort;
            attrs[2].id = SAI_VLAN_MEMBER_ATTR_VLAN_TAGGING_MODE;
            attrs[2].value.s32 = SAI_VLAN_TAGGING_MODE_UNTAGGED;

            EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->createVlanMember(member, m_switchId, 3, attrs));
        }

        // The learn flag calls VPP got for an interface, as (enable) values
        std::vector<bool> learnCalls(
                const std::string &hwif)
        {
            SWSS_LOG_ENTER();

            std::vector<bool> enables;

            for (const auto &call: vppMock().callsTo("set_l2_interface_flags", hwif))
            {
                EXPECT_EQ((uint32_t)VPP_BD_FLAG_LEARN, call.id);
                enables.push_back(call.flag);
            }

            return enables;
        }

        int32_t storedMode(
                sai_object_id_t bridgePort)
        {
            SWSS_LOG_ENTER();

            auto attr = m_sw->stored_attr(SAI_OBJECT_TYPE_BRIDGE_PORT,
                    sai_serialize_object_id(bridgePort), SAI_BRIDGE_PORT_ATTR_FDB_LEARNING_MODE);

            return attr ? attr->value.s32 : -1;
        }

        static constexpr uint16_t OTHER_VLAN_ID = 200;

        sai_object_id_t m_otherVlan = SAI_NULL_OBJECT_ID;
};

constexpr uint16_t SwitchVppBridgePortLearning::OTHER_VLAN_ID;

TEST_F(SwitchVppBridgePortLearning, DisableTurnsLearningOffOnEveryVlanMembership)
{
    addTaggedMember(m_bridgePort);
    addUntaggedMember(m_bridgePort);
    vppMock().calls.clear();

    EXPECT_EQ(SAI_STATUS_SUCCESS, setLearningMode(m_bridgePort, SAI_BRIDGE_PORT_FDB_LEARNING_MODE_DISABLE));

    EXPECT_EQ(std::vector<bool>({false}), learnCalls(MEMBER_HWIF));
    EXPECT_EQ(std::vector<bool>({false}), learnCalls(PORT_HWIF));
    EXPECT_EQ(SAI_BRIDGE_PORT_FDB_LEARNING_MODE_DISABLE, storedMode(m_bridgePort));
}

TEST_F(SwitchVppBridgePortLearning, HardwareTurnsLearningBackOn)
{
    addTaggedMember(m_bridgePort);
    ASSERT_EQ(SAI_STATUS_SUCCESS, setLearningMode(m_bridgePort, SAI_BRIDGE_PORT_FDB_LEARNING_MODE_DISABLE));
    vppMock().calls.clear();

    EXPECT_EQ(SAI_STATUS_SUCCESS, setLearningMode(m_bridgePort, SAI_BRIDGE_PORT_FDB_LEARNING_MODE_HW));

    EXPECT_EQ(std::vector<bool>({true}), learnCalls(MEMBER_HWIF));
}

/*
 * VPP turns learning on for an interface that joins a bridge domain, so a
 * bridge port with learning off joining a VLAN turns it off again.
 */
TEST_F(SwitchVppBridgePortLearning, BridgePortWithLearningOffJoiningAVlanTurnsItOff)
{
    ASSERT_EQ(SAI_STATUS_SUCCESS, setLearningMode(m_bridgePort, SAI_BRIDGE_PORT_FDB_LEARNING_MODE_DISABLE));
    EXPECT_TRUE(vppMock().calls.empty());

    addTaggedMember(m_bridgePort);
    addUntaggedMember(m_bridgePort);

    EXPECT_EQ(std::vector<bool>({false}), learnCalls(MEMBER_HWIF));
    EXPECT_EQ(std::vector<bool>({false}), learnCalls(PORT_HWIF));
    EXPECT_EQ(1u, vppMock().bdMembers[VLAN_ID].count(MEMBER_HWIF));
}

TEST_F(SwitchVppBridgePortLearning, BridgePortCreatedWithLearningOffJoiningAVlanTurnsItOff)
{
    auto bridgePort = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_BRIDGE_PORT, m_switchId);

    sai_attribute_t attrs[3];

    attrs[0].id = SAI_BRIDGE_PORT_ATTR_TYPE;
    attrs[0].value.s32 = SAI_BRIDGE_PORT_TYPE_PORT;
    attrs[1].id = SAI_BRIDGE_PORT_ATTR_PORT_ID;
    attrs[1].value.oid = m_port;
    attrs[2].id = SAI_BRIDGE_PORT_ATTR_FDB_LEARNING_MODE;
    attrs[2].value.s32 = SAI_BRIDGE_PORT_FDB_LEARNING_MODE_DISABLE;

    ASSERT_EQ(SAI_STATUS_SUCCESS,
            m_sw->create(SAI_OBJECT_TYPE_BRIDGE_PORT, sai_serialize_object_id(bridgePort), m_switchId, 3, attrs));

    addTaggedMember(bridgePort);

    EXPECT_EQ(std::vector<bool>({false}), learnCalls(MEMBER_HWIF));
}

TEST_F(SwitchVppBridgePortLearning, DefaultModeJoinsWithoutTouchingTheLearnFlag)
{
    addTaggedMember(m_bridgePort);

    EXPECT_TRUE(learnCalls(MEMBER_HWIF).empty());
}

TEST_F(SwitchVppBridgePortLearning, SettingTheModeItHasCallsNothing)
{
    addTaggedMember(m_bridgePort);
    vppMock().calls.clear();

    EXPECT_EQ(SAI_STATUS_SUCCESS, setLearningMode(m_bridgePort, SAI_BRIDGE_PORT_FDB_LEARNING_MODE_HW));
    EXPECT_EQ(SAI_STATUS_SUCCESS, setLearningMode(m_bridgePort, SAI_BRIDGE_PORT_FDB_LEARNING_MODE_FDB_NOTIFICATION));

    EXPECT_TRUE(learnCalls(MEMBER_HWIF).empty());
}

TEST_F(SwitchVppBridgePortLearning, OtherBridgePortsKeepLearning)
{
    addTaggedMember(m_bridgePort);
    addTaggedMember(m_otherBridgePort);
    vppMock().calls.clear();

    EXPECT_EQ(SAI_STATUS_SUCCESS, setLearningMode(m_bridgePort, SAI_BRIDGE_PORT_FDB_LEARNING_MODE_DISABLE));

    EXPECT_EQ(std::vector<bool>({false}), learnCalls(MEMBER_HWIF));
    EXPECT_TRUE(learnCalls(std::string(OTHER_PORT_HWIF) + ".100").empty());
}

/*
 * DROP also drops frames with an unknown source MAC, which VPP can't do per
 * interface: rejected, at create and on set, rather than half-honoured.
 */
TEST_F(SwitchVppBridgePortLearning, DropIsNotSupported)
{
    addTaggedMember(m_bridgePort);
    vppMock().calls.clear();

    EXPECT_EQ(SAI_STATUS_NOT_SUPPORTED, setLearningMode(m_bridgePort, SAI_BRIDGE_PORT_FDB_LEARNING_MODE_DROP));
    EXPECT_TRUE(learnCalls(MEMBER_HWIF).empty());
    EXPECT_EQ(-1, storedMode(m_bridgePort));

    auto bridgePort = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_BRIDGE_PORT, m_switchId);

    sai_attribute_t attrs[3];

    attrs[0].id = SAI_BRIDGE_PORT_ATTR_TYPE;
    attrs[0].value.s32 = SAI_BRIDGE_PORT_TYPE_PORT;
    attrs[1].id = SAI_BRIDGE_PORT_ATTR_PORT_ID;
    attrs[1].value.oid = m_otherPort;
    attrs[2].id = SAI_BRIDGE_PORT_ATTR_FDB_LEARNING_MODE;
    attrs[2].value.s32 = SAI_BRIDGE_PORT_FDB_LEARNING_MODE_DROP;

    EXPECT_EQ(SAI_STATUS_NOT_SUPPORTED,
            m_sw->create(SAI_OBJECT_TYPE_BRIDGE_PORT, sai_serialize_object_id(bridgePort), m_switchId, 3, attrs));
    EXPECT_EQ(0u, m_sw->m_objectHash.at(SAI_OBJECT_TYPE_BRIDGE_PORT).count(sai_serialize_object_id(bridgePort)));
}

TEST_F(SwitchVppBridgePortLearning, ModeVppFailsToSetIsNotStored)
{
    addTaggedMember(m_bridgePort);
    vppMock().l2FlagsResult = -1;

    EXPECT_EQ(SAI_STATUS_FAILURE, setLearningMode(m_bridgePort, SAI_BRIDGE_PORT_FDB_LEARNING_MODE_DISABLE));

    EXPECT_EQ(-1, storedMode(m_bridgePort));
}

TEST_F(SwitchVppBridgePortLearning, MemberVppFailsToTurnLearningOffForIsAFailure)
{
    ASSERT_EQ(SAI_STATUS_SUCCESS, setLearningMode(m_bridgePort, SAI_BRIDGE_PORT_FDB_LEARNING_MODE_DISABLE));
    vppMock().l2FlagsResult = -1;

    auto member = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_VLAN_MEMBER, m_switchId);

    sai_attribute_t attrs[3];

    attrs[0].id = SAI_VLAN_MEMBER_ATTR_VLAN_ID;
    attrs[0].value.oid = m_vlan;
    attrs[1].id = SAI_VLAN_MEMBER_ATTR_BRIDGE_PORT_ID;
    attrs[1].value.oid = m_bridgePort;
    attrs[2].id = SAI_VLAN_MEMBER_ATTR_VLAN_TAGGING_MODE;
    attrs[2].value.s32 = SAI_VLAN_TAGGING_MODE_TAGGED;

    EXPECT_EQ(SAI_STATUS_FAILURE, m_sw->createVlanMember(member, m_switchId, 3, attrs));
    EXPECT_EQ(0u, vppMock().bdMembers[VLAN_ID].count(MEMBER_HWIF));
    EXPECT_EQ(0u, vppMock().interfaces.count(MEMBER_HWIF));
    EXPECT_EQ(nullptr, m_sw->m_ifaceRegistry.findByHwif(MEMBER_HWIF));
    EXPECT_EQ(0u, m_sw->m_objectHash.at(SAI_OBJECT_TYPE_VLAN_MEMBER).count(sai_serialize_object_id(member)));

    attrs[0].value.oid = m_otherVlan;
    attrs[2].value.s32 = SAI_VLAN_TAGGING_MODE_UNTAGGED;

    EXPECT_EQ(SAI_STATUS_FAILURE, m_sw->createVlanMember(member, m_switchId, 3, attrs));
    EXPECT_EQ(0u, vppMock().bdMembers[OTHER_VLAN_ID].count(PORT_HWIF));
    EXPECT_EQ(0u, m_sw->m_objectHash.at(SAI_OBJECT_TYPE_VLAN_MEMBER).count(sai_serialize_object_id(member)));
}

TEST_F(SwitchVppBridgePortLearning, TunnelBridgePortModeIsOnlyStored)
{
    auto bridgePort = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_BRIDGE_PORT, m_switchId);

    sai_attribute_t battr;

    battr.id = SAI_BRIDGE_PORT_ATTR_TYPE;
    battr.value.s32 = SAI_BRIDGE_PORT_TYPE_TUNNEL;

    ASSERT_EQ(SAI_STATUS_SUCCESS,
            m_sw->create_internal(SAI_OBJECT_TYPE_BRIDGE_PORT, sai_serialize_object_id(bridgePort), m_switchId, 1, &battr));

    EXPECT_EQ(SAI_STATUS_SUCCESS, setLearningMode(bridgePort, SAI_BRIDGE_PORT_FDB_LEARNING_MODE_DISABLE));

    EXPECT_TRUE(vppMock().calls.empty());
    EXPECT_EQ(SAI_BRIDGE_PORT_FDB_LEARNING_MODE_DISABLE, storedMode(bridgePort));

    EXPECT_EQ(SAI_STATUS_NOT_SUPPORTED, setLearningMode(bridgePort, SAI_BRIDGE_PORT_FDB_LEARNING_MODE_DROP));
    EXPECT_EQ(SAI_BRIDGE_PORT_FDB_LEARNING_MODE_DISABLE, storedMode(bridgePort));
}

/*
 * SAI isolation groups, on sonic-ext's isolation-group feature: each binding
 * and membership on the VPP interface of every VLAN membership of the port.
 */
class SwitchVppIsolation : public SwitchVppBridgePortLearning
{
    protected:

        void SetUp() override
        {
            SwitchVppBridgePortLearning::SetUp();

            vppMock().enabledFeatures.insert("isolation-group");
        }

        sai_status_t createGroup(
                sai_object_id_t &group)
        {
            SWSS_LOG_ENTER();

            group = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_ISOLATION_GROUP, m_switchId);

            sai_attribute_t attr;

            attr.id = SAI_ISOLATION_GROUP_ATTR_TYPE;
            attr.value.s32 = SAI_ISOLATION_GROUP_TYPE_BRIDGE_PORT;

            return m_sw->create(SAI_OBJECT_TYPE_ISOLATION_GROUP, sai_serialize_object_id(group), m_switchId, 1, &attr);
        }

        sai_status_t addMember(
                sai_object_id_t group,
                sai_object_id_t object,
                sai_object_id_t &member)
        {
            SWSS_LOG_ENTER();

            member = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_ISOLATION_GROUP_MEMBER, m_switchId);

            sai_attribute_t attrs[2];

            attrs[0].id = SAI_ISOLATION_GROUP_MEMBER_ATTR_ISOLATION_GROUP_ID;
            attrs[0].value.oid = group;
            attrs[1].id = SAI_ISOLATION_GROUP_MEMBER_ATTR_ISOLATION_OBJECT;
            attrs[1].value.oid = object;

            return m_sw->create(SAI_OBJECT_TYPE_ISOLATION_GROUP_MEMBER, sai_serialize_object_id(member), m_switchId, 2, attrs);
        }

        sai_status_t bind(
                sai_object_id_t object,
                sai_object_id_t group)
        {
            SWSS_LOG_ENTER();

            auto type = m_sw->objectTypeQuery(object);

            sai_attribute_t attr;

            attr.id = type == SAI_OBJECT_TYPE_PORT
                ? (sai_attr_id_t)SAI_PORT_ATTR_ISOLATION_GROUP
                : (sai_attr_id_t)SAI_BRIDGE_PORT_ATTR_ISOLATION_GROUP;
            attr.value.oid = group;

            return m_sw->set(type, sai_serialize_object_id(object), &attr);
        }

        // The (interface, group id) pairs VPP got for an API, in order
        std::vector<std::pair<std::string, uint32_t>> calls(
                const std::string &api,
                bool flag = true)
        {
            SWSS_LOG_ENTER();

            std::vector<std::pair<std::string, uint32_t>> result;

            for (const auto &call: vppMock().calls)
            {
                if (call.api == api && call.flag == flag)
                {
                    result.push_back({call.name, call.id});
                }
            }

            return result;
        }

        // The (sw_if_index, group id) pairs VPP got for a by-index API, in order
        std::vector<std::pair<uint32_t, uint32_t>> indexCalls(
                const std::string &api,
                bool flag = true)
        {
            SWSS_LOG_ENTER();

            std::vector<std::pair<uint32_t, uint32_t>> result;

            for (const auto &call: vppMock().calls)
            {
                if (call.api == api && call.flag == flag)
                {
                    result.push_back({call.sub_id, call.id});
                }
            }

            return result;
        }

        sai_object_id_t createVniMapper(uint16_t vlanId, uint32_t vni)
        {
            SWSS_LOG_ENTER();

            auto mapper = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_TUNNEL_MAP, m_switchId);
            sai_attribute_t mattr;
            mattr.id = SAI_TUNNEL_MAP_ATTR_TYPE;
            mattr.value.s32 = SAI_TUNNEL_MAP_TYPE_VNI_TO_VLAN_ID;
            EXPECT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create(SAI_OBJECT_TYPE_TUNNEL_MAP, sai_serialize_object_id(mapper), m_switchId, 1, &mattr));

            auto entry = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_TUNNEL_MAP_ENTRY, m_switchId);
            sai_attribute_t eattrs[4];
            eattrs[0].id = SAI_TUNNEL_MAP_ENTRY_ATTR_TUNNEL_MAP_TYPE;
            eattrs[0].value.s32 = SAI_TUNNEL_MAP_TYPE_VNI_TO_VLAN_ID;
            eattrs[1].id = SAI_TUNNEL_MAP_ENTRY_ATTR_TUNNEL_MAP;
            eattrs[1].value.oid = mapper;
            eattrs[2].id = SAI_TUNNEL_MAP_ENTRY_ATTR_VNI_ID_KEY;
            eattrs[2].value.u32 = vni;
            eattrs[3].id = SAI_TUNNEL_MAP_ENTRY_ATTR_VLAN_ID_VALUE;
            eattrs[3].value.u16 = vlanId;
            EXPECT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create(SAI_OBJECT_TYPE_TUNNEL_MAP_ENTRY, sai_serialize_object_id(entry), m_switchId, 4, eattrs));
            return mapper;
        }

        sai_status_t createEvpnTunnel(
                sai_object_id_t mapper,
                uint32_t dstIpHostOrder,
                sai_object_id_t &tunnel,
                sai_object_id_t &tunnelBridgePort)
        {
            SWSS_LOG_ENTER();

            tunnel = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_TUNNEL, m_switchId);
            sai_object_id_t mappers[1] = {mapper};
            sai_attribute_t tattrs[5];
            tattrs[0].id = SAI_TUNNEL_ATTR_TYPE;
            tattrs[0].value.s32 = SAI_TUNNEL_TYPE_VXLAN;
            tattrs[1].id = SAI_TUNNEL_ATTR_PEER_MODE;
            tattrs[1].value.s32 = SAI_TUNNEL_PEER_MODE_P2P;
            tattrs[2].id = SAI_TUNNEL_ATTR_ENCAP_SRC_IP;
            tattrs[2].value.ipaddr.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
            tattrs[2].value.ipaddr.addr.ip4 = htonl(0x0a000001u);
            tattrs[3].id = SAI_TUNNEL_ATTR_ENCAP_DST_IP;
            tattrs[3].value.ipaddr.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
            tattrs[3].value.ipaddr.addr.ip4 = htonl(dstIpHostOrder);
            tattrs[4].id = SAI_TUNNEL_ATTR_DECAP_MAPPERS;
            tattrs[4].value.objlist.count = 1;
            tattrs[4].value.objlist.list = mappers;

            sai_status_t st = m_sw->create(SAI_OBJECT_TYPE_TUNNEL, sai_serialize_object_id(tunnel), m_switchId, 5, tattrs);
            if (st != SAI_STATUS_SUCCESS)
            {
                return st;
            }

            tunnelBridgePort = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_BRIDGE_PORT, m_switchId);
            sai_attribute_t battrs[2];
            battrs[0].id = SAI_BRIDGE_PORT_ATTR_TYPE;
            battrs[0].value.s32 = SAI_BRIDGE_PORT_TYPE_TUNNEL;
            battrs[1].id = SAI_BRIDGE_PORT_ATTR_TUNNEL_ID;
            battrs[1].value.oid = tunnel;
            return m_sw->create(SAI_OBJECT_TYPE_BRIDGE_PORT, sai_serialize_object_id(tunnelBridgePort), m_switchId, 2, battrs);
        }

        typedef std::vector<std::pair<std::string, uint32_t>> Calls;
        typedef std::vector<std::pair<uint32_t, uint32_t>> IndexCalls;

        static constexpr const char *OTHER_MEMBER_HWIF = "bobm10.100";
        static constexpr uint32_t NONE = ~0u;
};

constexpr const char *SwitchVppIsolation::OTHER_MEMBER_HWIF;
constexpr uint32_t SwitchVppIsolation::NONE;

/*
 * sonicExtFeatureEnabled() memorizes its answer in a process-wide cache, so
 * the feature-off case runs in a process of its own.
 */
TEST_F(SwitchVppIsolation, GroupNeedsTheSonicExtFeature)
{
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";

    ASSERT_EXIT({
        vppMock().enabledFeatures.clear();

        sai_object_id_t group;

        bool ok = createGroup(group) == SAI_STATUS_NOT_SUPPORTED &&
            calls("vpp_isolation_group_add_del").empty() &&
            m_sw->m_objectHash.at(SAI_OBJECT_TYPE_ISOLATION_GROUP).count(sai_serialize_object_id(group)) == 0;

        _exit(ok ? 0 : 1);
    }, ::testing::ExitedWithCode(0), "");
}

TEST_F(SwitchVppIsolation, GroupIsCreatedAndRemovedInVpp)
{
    sai_object_id_t group;

    ASSERT_EQ(SAI_STATUS_SUCCESS, createGroup(group));
    EXPECT_EQ(Calls({{"", 1}}), calls("vpp_isolation_group_add_del"));

    EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->remove(SAI_OBJECT_TYPE_ISOLATION_GROUP, sai_serialize_object_id(group)));
    EXPECT_EQ(Calls({{"", 1}}), calls("vpp_isolation_group_add_del", false));
}

TEST_F(SwitchVppIsolation, GroupVppRefusesToDeleteIsInUse)
{
    sai_object_id_t group;

    ASSERT_EQ(SAI_STATUS_SUCCESS, createGroup(group));
    vppMock().isolationGroupDelFailures.insert(1);

    EXPECT_EQ(SAI_STATUS_OBJECT_IN_USE, m_sw->remove(SAI_OBJECT_TYPE_ISOLATION_GROUP, sai_serialize_object_id(group)));
    EXPECT_EQ(1u, m_sw->m_objectHash.at(SAI_OBJECT_TYPE_ISOLATION_GROUP).count(sai_serialize_object_id(group)));
}

TEST_F(SwitchVppIsolation, UnknownGroupRemovalLeavesVppAlone)
{
    sai_object_id_t group;
    ASSERT_EQ(SAI_STATUS_SUCCESS, createGroup(group));
    auto unknown = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_ISOLATION_GROUP, m_switchId);
    vppMock().calls.clear();

    EXPECT_EQ(SAI_STATUS_ITEM_NOT_FOUND, m_sw->removeIsolationGroup(unknown));
    EXPECT_TRUE(calls("vpp_isolation_group_add_del", false).empty());
    EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->remove(SAI_OBJECT_TYPE_ISOLATION_GROUP, sai_serialize_object_id(group)));
}

/*
 * The MC-LAG shape: the peer link (the other port) bound to a group whose
 * member is a dual-homed port (the port), each across every VLAN it belongs to.
 */
TEST_F(SwitchVppIsolation, BindingAndMembershipCoverEveryVlanMembership)
{
    addTaggedMember(m_bridgePort);
    addUntaggedMember(m_bridgePort);
    addTaggedMember(m_otherBridgePort);
    vppMock().calls.clear();

    sai_object_id_t group, member;

    ASSERT_EQ(SAI_STATUS_SUCCESS, createGroup(group));
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(group, m_bridgePort, member));
    ASSERT_EQ(SAI_STATUS_SUCCESS, bind(m_otherBridgePort, group));

    auto members = calls("vpp_isolation_group_member_add_del");
    std::sort(members.begin(), members.end());

    EXPECT_EQ(Calls({{PORT_HWIF, 1}, {MEMBER_HWIF, 1}}), members);
    EXPECT_EQ(Calls({{OTHER_MEMBER_HWIF, 1}}), calls("vpp_isolation_group_bind"));
}

TEST_F(SwitchVppIsolation, UnbindingBindsToNoGroup)
{
    addTaggedMember(m_otherBridgePort);

    sai_object_id_t group;

    ASSERT_EQ(SAI_STATUS_SUCCESS, createGroup(group));
    ASSERT_EQ(SAI_STATUS_SUCCESS, bind(m_otherBridgePort, group));
    vppMock().calls.clear();

    EXPECT_EQ(SAI_STATUS_SUCCESS, bind(m_otherBridgePort, SAI_NULL_OBJECT_ID));

    EXPECT_EQ(Calls({{OTHER_MEMBER_HWIF, NONE}}), calls("vpp_isolation_group_bind"));
}

/*
 * orchagent sets up the group before the port-channels join each VLAN;
 * each VPP interface that appears later is bound and added as it joins.
 */
TEST_F(SwitchVppIsolation, VlanJoinedLaterIsBoundAndAdded)
{
    sai_object_id_t group, member;

    ASSERT_EQ(SAI_STATUS_SUCCESS, createGroup(group));
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(group, m_bridgePort, member));
    ASSERT_EQ(SAI_STATUS_SUCCESS, bind(m_otherBridgePort, group));
    EXPECT_TRUE(calls("vpp_isolation_group_member_add_del").empty());
    EXPECT_TRUE(calls("vpp_isolation_group_bind").empty());

    addTaggedMember(m_bridgePort);
    addTaggedMember(m_otherBridgePort);

    EXPECT_EQ(Calls({{MEMBER_HWIF, 1}}), calls("vpp_isolation_group_member_add_del"));
    EXPECT_EQ(Calls({{OTHER_MEMBER_HWIF, 1}}), calls("vpp_isolation_group_bind"));
}

TEST_F(SwitchVppIsolation, PortTypeGroupCoversTheBridgePortsOfThePort)
{
    addTaggedMember(m_bridgePort);
    vppMock().calls.clear();

    sai_object_id_t group, member;

    ASSERT_EQ(SAI_STATUS_SUCCESS, createGroup(group));
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(group, m_port, member));
    ASSERT_EQ(SAI_STATUS_SUCCESS, bind(m_otherPort, group));

    EXPECT_EQ(Calls({{MEMBER_HWIF, 1}}), calls("vpp_isolation_group_member_add_del"));

    addTaggedMember(m_otherBridgePort);

    EXPECT_EQ(Calls({{OTHER_MEMBER_HWIF, 1}}), calls("vpp_isolation_group_bind"));
}

TEST_F(SwitchVppIsolation, MemberRemovalLeavesTheGroupInVpp)
{
    addTaggedMember(m_bridgePort);

    sai_object_id_t group, member;

    ASSERT_EQ(SAI_STATUS_SUCCESS, createGroup(group));
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(group, m_bridgePort, member));
    vppMock().calls.clear();

    EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->remove(SAI_OBJECT_TYPE_ISOLATION_GROUP_MEMBER, sai_serialize_object_id(member)));

    EXPECT_EQ(Calls({{MEMBER_HWIF, 1}}), calls("vpp_isolation_group_member_add_del", false));
    EXPECT_EQ(0u, m_sw->m_objectHash.at(SAI_OBJECT_TYPE_ISOLATION_GROUP_MEMBER).count(sai_serialize_object_id(member)));
}

TEST_F(SwitchVppIsolation, MemberVppFailsToAddIsRolledBack)
{
    addTaggedMember(m_bridgePort);
    addUntaggedMember(m_bridgePort);

    sai_object_id_t group, member;

    ASSERT_EQ(SAI_STATUS_SUCCESS, createGroup(group));
    vppMock().isolationMemberAddFailures.insert(PORT_HWIF);
    vppMock().isolationMemberAddFailures.insert(MEMBER_HWIF);
    vppMock().calls.clear();

    EXPECT_EQ(SAI_STATUS_FAILURE, addMember(group, m_bridgePort, member));

    EXPECT_TRUE(calls("vpp_isolation_group_member_add_del", false).empty());
    EXPECT_EQ(0u, m_sw->m_objectHash.at(SAI_OBJECT_TYPE_ISOLATION_GROUP_MEMBER).count(sai_serialize_object_id(member)));

    // VPP adds the first interface and fails the second
    auto first = calls("vpp_isolation_group_member_add_del").front().first;

    vppMock().isolationMemberAddFailures.erase(first);
    vppMock().calls.clear();

    EXPECT_EQ(SAI_STATUS_FAILURE, addMember(group, m_bridgePort, member));

    EXPECT_EQ(Calls({{first, 1}}), calls("vpp_isolation_group_member_add_del", false));
    EXPECT_EQ(0u, m_sw->m_objectHash.at(SAI_OBJECT_TYPE_ISOLATION_GROUP_MEMBER).count(sai_serialize_object_id(member)));
}

TEST_F(SwitchVppIsolation, UnknownGroupIsRejected)
{
    addTaggedMember(m_otherBridgePort);

    auto group = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_ISOLATION_GROUP, m_switchId);
    sai_object_id_t member;

    EXPECT_NE(SAI_STATUS_SUCCESS, bind(m_otherBridgePort, group));
    EXPECT_NE(SAI_STATUS_SUCCESS, addMember(group, m_bridgePort, member));
    EXPECT_TRUE(calls("vpp_isolation_group_bind").empty());
}

TEST_F(SwitchVppIsolation, TunnelBridgePortCoversExistingAndLateTunnels)
{
    auto mapper = createVniMapper(VLAN_ID, 1000);
    sai_object_id_t tunnel = SAI_NULL_OBJECT_ID;
    sai_object_id_t tunnelBp = SAI_NULL_OBJECT_ID;
    ASSERT_EQ(SAI_STATUS_SUCCESS, createEvpnTunnel(mapper, 0x0a000002u, tunnel, tunnelBp));

    std::vector<uint32_t> sw_ifs;
    m_sw->m_tunnel_mgr.get_l2_vxlan_tunnel_if_indices(tunnel, sw_ifs);
    ASSERT_EQ(1u, sw_ifs.size());
    uint32_t firstSwIf = sw_ifs[0];

    sai_object_id_t group, member;
    ASSERT_EQ(SAI_STATUS_SUCCESS, createGroup(group));
    vppMock().calls.clear();

    EXPECT_EQ(SAI_STATUS_SUCCESS, addMember(group, tunnelBp, member));
    EXPECT_EQ(SAI_STATUS_SUCCESS, bind(tunnelBp, group));

    EXPECT_EQ(IndexCalls({{firstSwIf, 1}}), indexCalls("vpp_isolation_group_member_add_del_by_index"));
    EXPECT_EQ(IndexCalls({{firstSwIf, 1}}), indexCalls("vpp_isolation_group_bind_by_index"));

    // A second VNI added to the mapper creates another VPP VXLAN tunnel on the
    // same SAI P2P tunnel; it inherits both the binding and the membership.
    vppMock().calls.clear();
    auto lateEntry = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_TUNNEL_MAP_ENTRY, m_switchId);
    sai_attribute_t eattrs[4];
    eattrs[0].id = SAI_TUNNEL_MAP_ENTRY_ATTR_TUNNEL_MAP_TYPE;
    eattrs[0].value.s32 = SAI_TUNNEL_MAP_TYPE_VNI_TO_VLAN_ID;
    eattrs[1].id = SAI_TUNNEL_MAP_ENTRY_ATTR_TUNNEL_MAP;
    eattrs[1].value.oid = mapper;
    eattrs[2].id = SAI_TUNNEL_MAP_ENTRY_ATTR_VNI_ID_KEY;
    eattrs[2].value.u32 = 2000;
    eattrs[3].id = SAI_TUNNEL_MAP_ENTRY_ATTR_VLAN_ID_VALUE;
    eattrs[3].value.u16 = OTHER_VLAN_ID;
    ASSERT_EQ(SAI_STATUS_SUCCESS,
            m_sw->create(SAI_OBJECT_TYPE_TUNNEL_MAP_ENTRY, sai_serialize_object_id(lateEntry), m_switchId, 4, eattrs));

    m_sw->m_tunnel_mgr.get_l2_vxlan_tunnel_if_indices(tunnel, sw_ifs);
    ASSERT_EQ(2u, sw_ifs.size());
    uint32_t secondSwIf = sw_ifs[0] == firstSwIf ? sw_ifs[1] : sw_ifs[0];

    EXPECT_EQ(IndexCalls({{secondSwIf, 1}}), indexCalls("vpp_isolation_group_bind_by_index"));
    EXPECT_EQ(IndexCalls({{secondSwIf, 1}}), indexCalls("vpp_isolation_group_member_add_del_by_index"));

    vppMock().calls.clear();
    EXPECT_EQ(SAI_STATUS_SUCCESS, bind(tunnelBp, SAI_NULL_OBJECT_ID));
    EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->remove(SAI_OBJECT_TYPE_ISOLATION_GROUP_MEMBER, sai_serialize_object_id(member)));

    auto unbinds = indexCalls("vpp_isolation_group_bind_by_index");
    auto removes = indexCalls("vpp_isolation_group_member_add_del_by_index", false);
    std::sort(unbinds.begin(), unbinds.end());
    std::sort(removes.begin(), removes.end());
    IndexCalls wantUnbinds = {{firstSwIf, NONE}, {secondSwIf, NONE}};
    IndexCalls wantRemoves = {{firstSwIf, 1}, {secondSwIf, 1}};
    std::sort(wantUnbinds.begin(), wantUnbinds.end());
    std::sort(wantRemoves.begin(), wantRemoves.end());
    EXPECT_EQ(wantUnbinds, unbinds);
    EXPECT_EQ(wantRemoves, removes);
}

/*
 * Two SAI P2P tunnels to the same VTEP share the VPP tunnel of a VNI both
 * map. VPP binds an interface to one group, so the second owner may not
 * rebind it to another group, and a group both already hold is left alone.
 */
TEST_F(SwitchVppIsolation, SharedTunnelKeepsTheFirstOwnersBinding)
{
    auto mapper1 = createVniMapper(VLAN_ID, 1000);
    sai_object_id_t tunnel1 = SAI_NULL_OBJECT_ID;
    sai_object_id_t tunnelBp1 = SAI_NULL_OBJECT_ID;
    ASSERT_EQ(SAI_STATUS_SUCCESS, createEvpnTunnel(mapper1, 0x0a000002u, tunnel1, tunnelBp1));

    std::vector<uint32_t> sw_ifs;
    m_sw->m_tunnel_mgr.get_l2_vxlan_tunnel_if_indices(tunnel1, sw_ifs);
    ASSERT_EQ(1u, sw_ifs.size());
    uint32_t swIf = sw_ifs[0];

    sai_object_id_t group1, group2, member1, member2;
    ASSERT_EQ(SAI_STATUS_SUCCESS, createGroup(group1));
    ASSERT_EQ(SAI_STATUS_SUCCESS, createGroup(group2));
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(group1, tunnelBp1, member1));
    ASSERT_EQ(SAI_STATUS_SUCCESS, bind(tunnelBp1, group1));

    auto addVniEntry = [&](sai_object_id_t mapper, uint32_t vni) {
        auto entry = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_TUNNEL_MAP_ENTRY, m_switchId);
        sai_attribute_t eattrs[4];
        eattrs[0].id = SAI_TUNNEL_MAP_ENTRY_ATTR_TUNNEL_MAP_TYPE;
        eattrs[0].value.s32 = SAI_TUNNEL_MAP_TYPE_VNI_TO_VLAN_ID;
        eattrs[1].id = SAI_TUNNEL_MAP_ENTRY_ATTR_TUNNEL_MAP;
        eattrs[1].value.oid = mapper;
        eattrs[2].id = SAI_TUNNEL_MAP_ENTRY_ATTR_VNI_ID_KEY;
        eattrs[2].value.u32 = vni;
        eattrs[3].id = SAI_TUNNEL_MAP_ENTRY_ATTR_VLAN_ID_VALUE;
        eattrs[3].value.u16 = VLAN_ID;
        return m_sw->create(SAI_OBJECT_TYPE_TUNNEL_MAP_ENTRY, sai_serialize_object_id(entry), m_switchId, 4, eattrs);
    };

    // a second tunnel bound to another group cannot take over the shared tunnel
    auto mapper2 = createVniMapper(OTHER_VLAN_ID, 2000);
    sai_object_id_t tunnel2 = SAI_NULL_OBJECT_ID;
    sai_object_id_t tunnelBp2 = SAI_NULL_OBJECT_ID;
    ASSERT_EQ(SAI_STATUS_SUCCESS, createEvpnTunnel(mapper2, 0x0a000002u, tunnel2, tunnelBp2));
    ASSERT_EQ(SAI_STATUS_SUCCESS, bind(tunnelBp2, group2));

    vppMock().calls.clear();
    EXPECT_NE(SAI_STATUS_SUCCESS, addVniEntry(mapper2, 1000));
    EXPECT_TRUE(indexCalls("vpp_isolation_group_bind_by_index").empty());

    // one bound to the same group shares it without touching VPP again
    auto mapper3 = createVniMapper(OTHER_VLAN_ID, 3000);
    sai_object_id_t tunnel3 = SAI_NULL_OBJECT_ID;
    sai_object_id_t tunnelBp3 = SAI_NULL_OBJECT_ID;
    ASSERT_EQ(SAI_STATUS_SUCCESS, createEvpnTunnel(mapper3, 0x0a000002u, tunnel3, tunnelBp3));
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(group1, tunnelBp3, member2));
    ASSERT_EQ(SAI_STATUS_SUCCESS, bind(tunnelBp3, group1));

    vppMock().calls.clear();
    EXPECT_EQ(SAI_STATUS_SUCCESS, addVniEntry(mapper3, 1000));
    m_sw->m_tunnel_mgr.get_l2_vxlan_tunnel_if_indices(tunnel3, sw_ifs);
    EXPECT_NE(sw_ifs.end(), std::find(sw_ifs.begin(), sw_ifs.end(), swIf));
    for (const auto &call: indexCalls("vpp_isolation_group_bind_by_index"))
    {
        EXPECT_NE(swIf, call.first);
    }
    for (const auto &call: indexCalls("vpp_isolation_group_member_add_del_by_index"))
    {
        EXPECT_NE(swIf, call.first);
    }
}

TEST_F(SwitchVppIsolation, WarmBootRestoresIsolationGroupIdsWithoutCollision)
{
    sai_object_id_t firstGroup, secondGroup, thirdGroup;

    ASSERT_EQ(SAI_STATUS_SUCCESS, createGroup(firstGroup));
    ASSERT_EQ(SAI_STATUS_SUCCESS, createGroup(secondGroup));
    uint32_t firstId = m_sw->isolation_group_vpp_id(firstGroup);
    uint32_t secondId = m_sw->isolation_group_vpp_id(secondGroup);
    EXPECT_NE(firstId, secondId);

    // Simulate warm-boot reload: clear runtime map and rebuild from m_objectHash
    m_sw->m_isolation_group_ids.clear();
    m_sw->restoreIsolationGroups();

    EXPECT_EQ(firstId, m_sw->isolation_group_vpp_id(firstGroup));
    EXPECT_EQ(secondId, m_sw->isolation_group_vpp_id(secondGroup));

    vppMock().calls.clear();
    ASSERT_EQ(SAI_STATUS_SUCCESS, createGroup(thirdGroup));
    uint32_t thirdId = m_sw->isolation_group_vpp_id(thirdGroup);
    EXPECT_NE(firstId, thirdId);
    EXPECT_NE(secondId, thirdId);
    EXPECT_EQ(Calls({{"", thirdId}}), calls("vpp_isolation_group_add_del"));
}

TEST_F(SwitchVppIsolation, BindingVppFailsToApplyIsRolledBack)
{
    addTaggedMember(m_bridgePort);
    addUntaggedMember(m_bridgePort);

    sai_object_id_t firstGroup, secondGroup;

    ASSERT_EQ(SAI_STATUS_SUCCESS, createGroup(firstGroup));
    ASSERT_EQ(SAI_STATUS_SUCCESS, createGroup(secondGroup));
    ASSERT_EQ(SAI_STATUS_SUCCESS, bind(m_bridgePort, firstGroup));

    vppMock().isolationBindFailures.insert({PORT_HWIF, 2});
    vppMock().isolationBindFailures.insert({MEMBER_HWIF, 2});
    vppMock().calls.clear();

    EXPECT_EQ(SAI_STATUS_FAILURE, bind(m_bridgePort, secondGroup));
    auto first = calls("vpp_isolation_group_bind").front().first;

    vppMock().isolationBindFailures.erase({first, 2});
    vppMock().calls.clear();

    EXPECT_EQ(SAI_STATUS_FAILURE, bind(m_bridgePort, secondGroup));

    auto second = first == PORT_HWIF ? std::string(MEMBER_HWIF) : std::string(PORT_HWIF);

    EXPECT_EQ(Calls({{first, 2}, {second, 2}, {first, 1}}), calls("vpp_isolation_group_bind"));
    auto stored = m_sw->stored_attr(SAI_OBJECT_TYPE_BRIDGE_PORT,
            sai_serialize_object_id(m_bridgePort), SAI_BRIDGE_PORT_ATTR_ISOLATION_GROUP);
    ASSERT_NE(nullptr, stored);
    EXPECT_EQ(firstGroup, stored->value.oid);
}

TEST_F(SwitchVppIsolation, VlanJoinedLaterRollsBackWhenIsolationFails)
{
    sai_object_id_t group, member;

    ASSERT_EQ(SAI_STATUS_SUCCESS, createGroup(group));
    ASSERT_EQ(SAI_STATUS_SUCCESS, bind(m_bridgePort, group));
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(group, m_bridgePort, member));
    vppMock().isolationMemberAddFailures.insert(MEMBER_HWIF);
    vppMock().calls.clear();

    auto vlanMember = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_VLAN_MEMBER, m_switchId);

    sai_attribute_t attrs[3];

    attrs[0].id = SAI_VLAN_MEMBER_ATTR_VLAN_ID;
    attrs[0].value.oid = m_vlan;
    attrs[1].id = SAI_VLAN_MEMBER_ATTR_BRIDGE_PORT_ID;
    attrs[1].value.oid = m_bridgePort;
    attrs[2].id = SAI_VLAN_MEMBER_ATTR_VLAN_TAGGING_MODE;
    attrs[2].value.s32 = SAI_VLAN_TAGGING_MODE_TAGGED;

    EXPECT_EQ(SAI_STATUS_FAILURE, m_sw->createVlanMember(vlanMember, m_switchId, 3, attrs));

    EXPECT_EQ(Calls({{MEMBER_HWIF, 1}, {MEMBER_HWIF, NONE}}), calls("vpp_isolation_group_bind"));
    EXPECT_EQ(0u, vppMock().bdMembers[VLAN_ID].count(MEMBER_HWIF));
    EXPECT_EQ(0u, vppMock().interfaces.count(MEMBER_HWIF));
    EXPECT_EQ(0u, m_sw->m_objectHash.at(SAI_OBJECT_TYPE_VLAN_MEMBER).count(sai_serialize_object_id(vlanMember)));
}
