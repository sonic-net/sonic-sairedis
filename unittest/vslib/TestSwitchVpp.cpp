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
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <set>
#include <vector>
#include <unistd.h>

using namespace saivs;

namespace
{
    const uint32_t MISS = ~0u;

    // IPv4 API routes VPP holds: (table, prefix) -> next hop type
    std::map<std::pair<uint32_t, std::string>, int> g_ip4Routes;

    // the IP2ME address set: the destinations the IP2ME policer meters on the punt path
    std::set<std::string> g_ip2meAddrs;

    uint32_t g_nextSwIfIndex = 100;
    int g_l2fibByIndexDelResult = 0;

    // destination address of each VXLAN tunnel the stub created, by sw_if_index
    std::map<uint32_t, std::string> g_vxlanTunnelDst;

    std::string macStr(
            const uint8_t *mac)
    {
        SWSS_LOG_ENTER();

        char buf[18];

        snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

        return buf;
    }

    // position of the first recorded call matching api and name, or -1
    int vppCallIndex(
            const std::string& api,
            const std::string& name)
    {
        SWSS_LOG_ENTER();

        for (size_t i = 0; i < vppMock().calls.size(); i++)
        {
            if (vppMock().calls[i].api == api && vppMock().calls[i].name == name)
            {
                return (int)i;
            }
        }

        return -1;
    }

    std::vector<VppCall> vppCallsTo(
            const std::string& api)
    {
        SWSS_LOG_ENTER();

        std::vector<VppCall> calls;

        for (auto& call: vppMock().calls)
        {
            if (call.api == api)
            {
                calls.push_back(call);
            }
        }

        return calls;
    }
}

extern "C" {

int __wrap_ip_route_add_del(vpp_ip_route_t *route, bool is_add)
{
    SWSS_LOG_ENTER();

    if (route->prefix_addr.sa_family != AF_INET || route->nexthop_cnt < 1)
    {
        return 0;
    }

    char addr[INET_ADDRSTRLEN];

    inet_ntop(AF_INET, &route->prefix_addr.addr.ip4.sin_addr, addr, sizeof(addr));

    std::string prefix = std::string(addr) + "/" + std::to_string(route->prefix_len);

    vppMock().calls.push_back({"ip_route_add_del", prefix, route->vrf_id, 0, is_add});

    // VPP has no table to add the route to, or no route to delete (table 0 always exists)
    if (route->vrf_id != 0 && vppMock().vrfTables.count(route->vrf_id) == 0)
    {
        return -1;
    }

    auto key = std::make_pair(route->vrf_id, prefix);

    if (is_add)
    {
        g_ip4Routes[key] = route->nexthop[0].type;
        return 0;
    }

    return g_ip4Routes.erase(key) == 1 ? 0 : -1;
}

int __wrap_vpp_sonic_ext_copp_ip2me_addr_add_del(uint32_t addr, bool is_add)
{
    SWSS_LOG_ENTER();

    char text[INET_ADDRSTRLEN];

    inet_ntop(AF_INET, &addr, text, sizeof(text));

    // a set: adding an address twice or deleting a missing one is a no-op
    if (is_add)
    {
        g_ip2meAddrs.insert(text);
    }
    else
    {
        g_ip2meAddrs.erase(text);
    }

    return 0;
}

int __wrap_l2fib_add_del(const char *hwif_name, const uint8_t *mac, uint32_t bd_id, bool is_add, bool is_static_mac)
{
    SWSS_LOG_ENTER();

    vppMock().calls.push_back({"l2fib_add_del", std::string(hwif_name ? hwif_name : "") + " " + macStr(mac), bd_id, is_static_mac, is_add});
    return 0;
}

int __wrap_l2fib_flush_all()
{
    SWSS_LOG_ENTER();

    vppMock().calls.push_back({"l2fib_flush_all", "", 0, 0, false});
    return 0;
}

int __wrap_l2fib_flush_int(const char *hwif_name)
{
    SWSS_LOG_ENTER();

    vppMock().calls.push_back({"l2fib_flush_int", hwif_name ? hwif_name : "", 0, 0, false});
    return 0;
}

int __wrap_l2fib_flush_bd(uint32_t bd_id)
{
    SWSS_LOG_ENTER();

    vppMock().calls.push_back({"l2fib_flush_bd", "", bd_id, 0, false});
    return 0;
}

/*
 * L2 VXLAN tunnels: name is the sw_if_index, id the bridge domain, sub_id the
 * split horizon group, flag enable. vpp_vxlan_tunnel_add_del: id is the VNI,
 * sub_id the sw_if_index a create hands out.
 */
int __wrap_vpp_vxlan_tunnel_add_del(vpp_vxlan_tunnel_t *tunnel, bool is_add, uint32_t *sw_if_index)
{
    SWSS_LOG_ENTER();

    if (is_add)
    {
        *sw_if_index = g_nextSwIfIndex++;

        char dst[INET6_ADDRSTRLEN];

        vpp_ip_addr_t_to_string(&tunnel->dst_address, dst, sizeof(dst));
        g_vxlanTunnelDst[*sw_if_index] = dst;
    }

    vppMock().calls.push_back({"vpp_vxlan_tunnel_add_del", "", tunnel->vni, is_add ? *sw_if_index : 0, is_add});
    return 0;
}

int __wrap_set_sw_interface_l2_bridge_by_index(uint32_t sw_if_index, uint32_t bridge_id, bool l2_enable, uint32_t port_type)
{
    SWSS_LOG_ENTER();

    // split horizon group 0
    vppMock().calls.push_back({"set_sw_interface_l2_bridge_shg_by_index", std::to_string(sw_if_index), bridge_id, 0, l2_enable});
    return 0;
}

int __wrap_set_sw_interface_l2_bridge_shg_by_index(uint32_t sw_if_index, uint32_t bridge_id, bool l2_enable, uint32_t port_type, uint8_t shg)
{
    SWSS_LOG_ENTER();

    vppMock().calls.push_back({"set_sw_interface_l2_bridge_shg_by_index", std::to_string(sw_if_index), bridge_id, shg, l2_enable});
    return 0;
}

int __wrap_set_l2_interface_flags_by_index(uint32_t sw_if_index, vpp_bd_flags_t flags, bool enable)
{
    SWSS_LOG_ENTER();

    vppMock().calls.push_back({"set_l2_interface_flags_by_index", std::to_string(sw_if_index), (uint32_t)flags, 0, enable});
    return 0;
}

int __wrap_l2fib_add_del_by_index(uint32_t sw_if_index, const uint8_t *mac, uint32_t bd_id, bool is_add, bool is_static_mac)
{
    SWSS_LOG_ENTER();

    vppMock().calls.push_back({"l2fib_add_del_by_index", std::to_string(sw_if_index) + " " + macStr(mac), bd_id, is_static_mac, is_add});
    return is_add ? 0 : g_l2fibByIndexDelResult;
}

}

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

TEST_F(SwitchVppDhcpVlanPunt, ClassifiesCanonicalDhcpV6AndLeavesExtensionsForProvider)
{
    addMember("bobm0", false);
    addMember("bobm1", true);

    uint32_t puntNext = vppMock().nodeNext(
            "l2-input-classify", "linux-cp-punt");
    uint32_t fixupNext = vppMock().nodeNext(
            "l2-input-classify", "sonic-ext-l2-trap-fixup");
    ASSERT_NE(~0u, puntNext);
    ASSERT_NE(~0u, fixupNext);

    // Preserve the VLAN DHCPv6 classifiers for canonical client packets.
    EXPECT_EQ(puntNext, vppMock().classifyFrame(
            "bobm0", 1, dhcp6(false)));
    EXPECT_EQ(fixupNext, vppMock().classifyFrame(
            "bobm1.100", 2, dhcp6(true)));

    // Fixed-offset classify deliberately misses extension headers; the
    // provider's semantic parser can handle these after VLAN admission.
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

/*
 * The L2 punt classify sessions trap DHCPv4, ARP and IPv6 multicast to the
 * control plane. With sonic-ext's l2-trap-learn on, they send the frame to
 * sonic-ext-l2-trap-learn, so that VPP learns its source MAC, as an ASIC does;
 * LLDP's session never does. The first VLAN member sets up the sessions of
 * untagged and tagged members alike, once per process, and
 * sonicExtFeatureEnabled() keeps what VPP reports, so each case runs in a
 * process of its own.
 */
class SwitchVppL2TrapLearn : public SwitchVppSubIfAdminState
{
    protected:

        // the next index of each node a punt session can send a frame to
        enum : uint32_t
        {
            PUNT = 1,       // linux-cp-punt
            TRAP_FIXUP,     // sonic-ext-l2-trap-fixup
            VLAN_FILTER,    // sonic-ext-l2-vlan-filter
            TRAP_LEARN,     // sonic-ext-l2-trap-learn
        };

        // the nexts of the punt sessions, by what they match
        using Nexts = std::map<std::string, std::vector<uint32_t>>;

        // What a punt session matches: the ethertype of a tagged frame
        // follows its 802.1Q tag
        static std::string matched(
                const std::vector<uint8_t>& match)
        {
            SWSS_LOG_ENTER();

            auto u16 = [&match](size_t at) {
                return at + 1 < match.size() ? (match[at] << 8) | match[at + 1] : 0;
            };

            bool tagged = u16(16) != 0;
            std::string member = tagged ? "tagged " : "untagged ";

            if (u16(0) == 0x3333)
            {
                return member + "IPv6 multicast";
            }

            switch (tagged ? u16(16) : u16(12))
            {
                case 0x0800:
                    return member + "DHCPv4";
                case 0x0806:
                    return member + "ARP";
                case 0x88cc:
                    return "LLDP";
                case 0x8100:
                    return "VLAN filter";
                default:
                    return "other";
            }
        }

        static Nexts puntNexts()
        {
            SWSS_LOG_ENTER();

            Nexts nexts;

            for (const auto& session: vppMock().classifySessions)
            {
                nexts[matched(session.match)].push_back(session.hit_next_index);
            }

            return nexts;
        }

        // the nexts, by the name of each node
        std::string str(
                const Nexts& nexts) const
        {
            SWSS_LOG_ENTER();

            std::map<uint32_t, std::string> names;

            for (const auto& node: NODES)
            {
                names[node.second] = node.first;
            }

            std::string s;

            for (const auto& kv: nexts)
            {
                s += "  " + kv.first + ":";

                for (auto next: kv.second)
                {
                    s += " " + (names.count(next) ? names[next] : std::to_string(next));
                }

                s += "\n";
            }

            return s;
        }

        // The punt sessions the first VLAN member sets up have the nexts
        // expected, with these sonic-ext features on and these nodes in VPP
        void expectPuntNexts(
                const std::set<std::string>& features,
                const std::map<std::string, uint32_t>& nodes,
                const Nexts& expected)
        {
            SWSS_LOG_ENTER();

            ::testing::FLAGS_gtest_death_test_style = "threadsafe";
            ASSERT_EXIT({
                vppMock().enabledFeatures = features;
                vppMock().classifyAvailable = true;
                for (const auto& node: nodes)
                {
                    vppMock().nodeNexts[std::make_pair(
                            std::string("l2-input-classify"), node.first)] = node.second;
                }

                addTaggedMember(m_bridgePort);

                auto nexts = puntNexts();

                if (nexts != expected)
                {
                    fprintf(stderr, "punt session nexts:\n%sexpected:\n%s",
                            str(nexts).c_str(), str(expected).c_str());
                }

                _exit(nexts == expected && !::testing::Test::HasFailure() ? 0 : 1);
            }, ::testing::ExitedWithCode(0), "");
        }

        const std::set<std::string> FEATURES = {"l2-trap-fixup", "l2-trap-learn", "l2-vlan-filter"};

        const std::map<std::string, uint32_t> NODES = {
            {"linux-cp-punt", PUNT},
            {"sonic-ext-l2-trap-fixup", TRAP_FIXUP},
            {"sonic-ext-l2-vlan-filter", VLAN_FILTER},
            {"sonic-ext-l2-trap-learn", TRAP_LEARN},
        };

        // the nexts without sonic-ext-l2-trap-learn, as before it; tagged
        // IPv6 multicast has a session in each of its two tables
        const Nexts NEXTS_WITHOUT_TRAP_LEARN = {
            {"untagged DHCPv4", {PUNT}},
            {"untagged ARP", {PUNT}},
            {"untagged IPv6 multicast", {PUNT}},
            {"tagged DHCPv4", {TRAP_FIXUP}},
            {"tagged ARP", {TRAP_FIXUP}},
            {"tagged IPv6 multicast", {TRAP_FIXUP, TRAP_FIXUP}},
            {"LLDP", {PUNT}},
            {"VLAN filter", {VLAN_FILTER}},
        };
};

TEST_F(SwitchVppL2TrapLearn, TrapsGoToTrapLearnWhenItIsOn)
{
    expectPuntNexts(FEATURES, NODES, {
        {"untagged DHCPv4", {TRAP_LEARN}},
        {"untagged ARP", {TRAP_LEARN}},
        {"untagged IPv6 multicast", {TRAP_LEARN}},
        {"tagged DHCPv4", {TRAP_LEARN}},
        {"tagged ARP", {TRAP_LEARN}},
        {"tagged IPv6 multicast", {TRAP_LEARN, TRAP_LEARN}},
        {"LLDP", {PUNT}},
        {"VLAN filter", {VLAN_FILTER}},
    });
}

TEST_F(SwitchVppL2TrapLearn, TrapsKeepTheirNextsWhenTrapLearnIsOff)
{
    expectPuntNexts({"l2-trap-fixup", "l2-vlan-filter"}, NODES, NEXTS_WITHOUT_TRAP_LEARN);
}

/*
 * A sonic-ext plugin that reports l2-trap-learn on without having its node.
 */
TEST_F(SwitchVppL2TrapLearn, TrapsKeepTheirNextsWhenVppHasNoTrapLearnNode)
{
    auto nodes = NODES;

    nodes.erase("sonic-ext-l2-trap-learn");

    expectPuntNexts(FEATURES, nodes, NEXTS_WITHOUT_TRAP_LEARN);
}

/*
 * sonic-ext-l2-trap-learn sends its copy of the frame to
 * sonic-ext-l2-trap-fixup, so it is not used without it.
 */
TEST_F(SwitchVppL2TrapLearn, TrapsDoNotGoToTrapLearnWithoutTrapFixup)
{
    expectPuntNexts({"l2-trap-learn", "l2-vlan-filter"}, NODES, {
        {"untagged DHCPv4", {PUNT}},
        {"untagged ARP", {PUNT}},
        {"untagged IPv6 multicast", {PUNT}},
        {"LLDP", {PUNT}},
        {"VLAN filter", {VLAN_FILTER}},
    });
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

/*
 * SAI_HOSTIF_TRAP_TYPE_DHCP packet action on VLAN 100: an untagged member on
 * one port, a tagged one on another, and the VLAN's BVI. Each test asks what
 * VPP would do with a DHCPv4 client broadcast on a member (punted by the l2
 * input classifier, or let through to the flood) and with the BVI's flood
 * copy (received, if the BVI's table has 255.255.255.255/32 as a local
 * route, or dropped by the route VPP installs there by default).
 *
 * The classify tables live in VPP, so they outlive a SwitchVpp instance just
 * as the stubbed tables outlive a test. The tests only look at the end state.
 *
 * The unit tests are built with access control checking disabled, so the
 * fixture reaches the protected SwitchVpp members directly.
 */
class SwitchVppDhcpTrap : public ::testing::Test
{
    protected:

        enum : uint32_t
        {
            PUNT = 1,
            TRAP_FIXUP = 2,
            VLAN_FILTER = 3,
        };

        void SetUp() override
        {
            auto sc = std::make_shared<SwitchConfig>(0, "");

            sc->m_saiSwitchType = SAI_SWITCH_TYPE_NPU;
            sc->m_switchType = SAI_VS_SWITCH_TYPE_VPP;
            sc->m_bootType = SAI_VS_BOOT_TYPE_COLD;
            sc->m_useTapDevice = false;
            sc->m_laneMap = LaneMap::getDefaultLaneMap(0);
            sc->m_eventQueue = std::make_shared<EventQueue>(std::make_shared<Signal>());

            auto scc = std::make_shared<SwitchConfigContainer>();

            scc->insert(sc);

            m_mgr = std::make_shared<RealObjectIdManager>(0, scc);
            m_sw = std::make_shared<SwitchVpp>(m_switchId, m_mgr, sc);

            m_vlan = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_VLAN, m_switchId);

            sai_attribute_t vattrs[2];

            vattrs[0].id = SAI_VLAN_ATTR_VLAN_ID;
            vattrs[0].value.u16 = VLAN_ID;
            // proxy ARP on the VLAN, so the members also punt broadcast ARP
            vattrs[1].id = SAI_VLAN_ATTR_BROADCAST_FLOOD_CONTROL_TYPE;
            vattrs[1].value.s32 = SAI_VLAN_FLOOD_CONTROL_TYPE_NONE;

            ASSERT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create_internal(SAI_OBJECT_TYPE_VLAN, sai_serialize_object_id(m_vlan), m_switchId, 2, vattrs));

            // what switch initialization records, and the BVI path reads
            m_defaultVr = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, m_switchId);

            sai_attribute_t sattr;

            sattr.id = SAI_SWITCH_ATTR_DEFAULT_VIRTUAL_ROUTER_ID;
            sattr.value.oid = m_defaultVr;

            ASSERT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create_internal(SAI_OBJECT_TYPE_SWITCH, sai_serialize_object_id(m_switchId), m_switchId, 1, &sattr));

            vppMock().reset();
            vppMock().enabledFeatures = {"l2-trap-fixup", "l2-vlan-filter"};
            vppMock().classifyAvailable = true;
            vppMock().nodeNexts = {
                {{"l2-input-classify", "linux-cp-punt"}, PUNT},
                {{"l2-input-classify", "sonic-ext-l2-trap-fixup"}, TRAP_FIXUP},
                {{"l2-input-classify", "sonic-ext-l2-vlan-filter"}, VLAN_FILTER},
            };
            vppMock().l2ClassifyTables.clear();
            g_ip4Routes.clear();
            g_ip2meAddrs.clear();
        }

        void TearDown() override
        {
            m_sw.reset();

            vppMock().reset();
        }

        sai_object_id_t createTrap(
                sai_packet_action_t action)
        {
            SWSS_LOG_ENTER();

            auto oid = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_HOSTIF_TRAP, m_switchId);

            sai_attribute_t attrs[2];

            attrs[0].id = SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE;
            attrs[0].value.s32 = SAI_HOSTIF_TRAP_TYPE_DHCP;
            attrs[1].id = SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION;
            attrs[1].value.s32 = action;

            EXPECT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create(SAI_OBJECT_TYPE_HOSTIF_TRAP, sai_serialize_object_id(oid), m_switchId, 2, attrs));

            return oid;
        }

        void setTrapAction(
                sai_object_id_t trap,
                sai_packet_action_t action)
        {
            SWSS_LOG_ENTER();

            sai_attribute_t attr;

            attr.id = SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION;
            attr.value.s32 = action;

            EXPECT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->set(SAI_OBJECT_TYPE_HOSTIF_TRAP, sai_serialize_object_id(trap), &attr));
        }

        void removeTrap(
                sai_object_id_t trap)
        {
            SWSS_LOG_ENTER();

            EXPECT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->remove(SAI_OBJECT_TYPE_HOSTIF_TRAP, sai_serialize_object_id(trap)));
        }

        void addMember(
                const char *hwif,
                bool tagged)
        {
            SWSS_LOG_ENTER();

            auto port = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_PORT, m_switchId);

            ASSERT_NE(nullptr, m_sw->m_ifaceRegistry.addPhysicalPort(hwif, std::string("os_") + hwif, port));

            auto bridgePort = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_BRIDGE_PORT, m_switchId);

            sai_attribute_t battrs[2];

            battrs[0].id = SAI_BRIDGE_PORT_ATTR_TYPE;
            battrs[0].value.s32 = SAI_BRIDGE_PORT_TYPE_PORT;
            battrs[1].id = SAI_BRIDGE_PORT_ATTR_PORT_ID;
            battrs[1].value.oid = port;

            ASSERT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create_internal(SAI_OBJECT_TYPE_BRIDGE_PORT, sai_serialize_object_id(bridgePort), m_switchId, 2, battrs));

            sai_attribute_t mattrs[3];

            mattrs[0].id = SAI_VLAN_MEMBER_ATTR_VLAN_ID;
            mattrs[0].value.oid = m_vlan;
            mattrs[1].id = SAI_VLAN_MEMBER_ATTR_BRIDGE_PORT_ID;
            mattrs[1].value.oid = bridgePort;
            mattrs[2].id = SAI_VLAN_MEMBER_ATTR_VLAN_TAGGING_MODE;
            mattrs[2].value.s32 = tagged ? SAI_VLAN_TAGGING_MODE_TAGGED : SAI_VLAN_TAGGING_MODE_UNTAGGED;

            ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->vpp_create_vlan_member(3, mattrs));
        }

        void addMembers()
        {
            SWSS_LOG_ENTER();

            addMember("bobm0", false);
            addMember("bobm1", true);
        }

        void createBvi()
        {
            SWSS_LOG_ENTER();

            m_rif = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_ROUTER_INTERFACE, m_switchId);

            sai_attribute_t attrs[4];

            attrs[0].id = SAI_ROUTER_INTERFACE_ATTR_TYPE;
            attrs[0].value.s32 = SAI_ROUTER_INTERFACE_TYPE_VLAN;
            attrs[1].id = SAI_ROUTER_INTERFACE_ATTR_VLAN_ID;
            attrs[1].value.oid = m_vlan;
            attrs[2].id = SAI_ROUTER_INTERFACE_ATTR_SRC_MAC_ADDRESS;
            memcpy(attrs[2].value.mac, BVI_MAC, sizeof(sai_mac_t));
            attrs[3].id = SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID;
            attrs[3].value.oid = m_defaultVr;

            SwitchVpp::RifCreateState state;

            ASSERT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create_internal(SAI_OBJECT_TYPE_ROUTER_INTERFACE, sai_serialize_object_id(m_rif), m_switchId, 4, attrs));
            ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->vpp_create_bvi_interface(4, attrs, state));
        }

        static std::vector<uint8_t> ip4Udp(
                uint8_t dst_last_octet_or_bcast,
                uint16_t dport)
        {
            SWSS_LOG_ENTER();

            std::vector<uint8_t> ip(28, 0);

            ip[0] = 0x45;                                   // IPv4, header length 5
            ip[9] = 17;                                     // UDP
            ip[16] = ip[17] = ip[18] = 0xff;                // destination 255.255.255.x
            ip[19] = dst_last_octet_or_bcast;
            ip[20] = 0; ip[21] = 68;                        // sport 68
            ip[22] = (uint8_t)(dport >> 8);
            ip[23] = (uint8_t)(dport & 0xff);

            return ip;
        }

        // a DHCPv4 client broadcast (DISCOVER or REQUEST) as it arrives on a member
        static std::vector<uint8_t> dhcpOnWire(
                bool tagged)
        {
            SWSS_LOG_ENTER();

            std::vector<uint8_t> f = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x02, 0, 0, 0, 0, 0x01};

            if (tagged)
            {
                f.insert(f.end(), {0x81, 0x00, 0x00, (uint8_t)VLAN_ID});
            }

            f.insert(f.end(), {0x08, 0x00});

            auto ip = ip4Udp(0xff, 67);

            f.insert(f.end(), ip.begin(), ip.end());

            return f;
        }

        // a broadcast ARP request as it arrives on a member
        static std::vector<uint8_t> arpOnWire(
                bool tagged)
        {
            SWSS_LOG_ENTER();

            std::vector<uint8_t> f = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x02, 0, 0, 0, 0, 0x01};

            if (tagged)
            {
                f.insert(f.end(), {0x81, 0x00, 0x00, (uint8_t)VLAN_ID});
            }

            f.insert(f.end(), {0x08, 0x06});
            f.resize(f.size() + 28, 0);

            return f;
        }

        // l2-input-classify's verdict on a frame: the hit next, or MISS (bridged and flooded)
        static uint32_t onMember(
                const char *hwif,
                bool tagged,
                const std::vector<uint8_t>& frame)
        {
            SWSS_LOG_ENTER();

            bool ip4 = !tagged && frame[12] == 0x08 && frame[13] == 0x00;

            // an untagged IPv4 frame selects the ip4 slot, any tagged frame the other slot
            return vppMock().classifyFrame(hwif, ip4 ? 0 : 2, frame);
        }

        // whether table vrf_id receives a limited broadcast, rather than dropping it
        static bool receivesLimitedBroadcast(
                uint32_t vrf_id)
        {
            SWSS_LOG_ENTER();

            if (vrf_id != 0 && vppMock().vrfTables.count(vrf_id) == 0)
            {
                return false;
            }

            auto it = g_ip4Routes.find({vrf_id, "255.255.255.255/32"});

            return it != g_ip4Routes.end() && it->second == VPP_NEXTHOP_LOCAL;
        }

        // whether the IP2ME policer meters limited broadcasts punted to the CPU
        static bool policesLimitedBroadcast()
        {
            SWSS_LOG_ENTER();

            return g_ip2meAddrs.count("255.255.255.255") == 1;
        }

        static uint32_t memberPunt()
        {
            SWSS_LOG_ENTER();

            return PUNT;
        }

        static uint32_t tagFixup()
        {
            SWSS_LOG_ENTER();

            return TRAP_FIXUP;
        }

        // TRAP: punted on both members, so the BVI never sees it, and limited
        // broadcasts on L3 tables stay at VPP's default drop
        void expectTrap()
        {
            SWSS_LOG_ENTER();

            EXPECT_EQ(memberPunt(), onMember("bobm0", false, dhcpOnWire(false)));
            EXPECT_EQ(tagFixup(), onMember("bobm1.100", true, dhcpOnWire(true)));
            EXPECT_FALSE(receivesLimitedBroadcast(0));
        }

        // COPY: flooded on both members, and the BVI's copy received, under the IP2ME policer
        void expectCopy()
        {
            SWSS_LOG_ENTER();

            EXPECT_EQ(MISS, onMember("bobm0", false, dhcpOnWire(false)));
            EXPECT_EQ(MISS, onMember("bobm1.100", true, dhcpOnWire(true)));
            EXPECT_TRUE(receivesLimitedBroadcast(0));
            EXPECT_TRUE(policesLimitedBroadcast());
        }

        static constexpr uint16_t VLAN_ID = 100;
        static constexpr uint32_t VRF_TABLE = 1001;
        static constexpr uint8_t BVI_MAC[6] = { 0x02, 0x00, 0x00, 0x00, 0x01, 0x00 };

        const sai_object_id_t m_switchId = 0x2100000000;

        std::shared_ptr<RealObjectIdManager> m_mgr;

        std::shared_ptr<SwitchVpp> m_sw;

        sai_object_id_t m_vlan = SAI_NULL_OBJECT_ID;
        sai_object_id_t m_defaultVr = SAI_NULL_OBJECT_ID;
        sai_object_id_t m_rif = SAI_NULL_OBJECT_ID;
};

constexpr uint16_t SwitchVppDhcpTrap::VLAN_ID;
constexpr uint32_t SwitchVppDhcpTrap::VRF_TABLE;
constexpr uint8_t SwitchVppDhcpTrap::BVI_MAC[6];

// The first test to create classify tables, so it also covers a COPY trap that predates them.
TEST_F(SwitchVppDhcpTrap, CopyBeforeAnyMemberFloodsAndTheBviCopyIsReceived)
{
    createTrap(SAI_PACKET_ACTION_COPY);
    addMembers();
    createBvi();

    expectCopy();
}

TEST_F(SwitchVppDhcpTrap, TrapPuntsOnTheMembers)
{
    createTrap(SAI_PACKET_ACTION_TRAP);
    addMembers();
    createBvi();

    expectTrap();
}

TEST_F(SwitchVppDhcpTrap, NoDhcpTrapDefaultsToTrap)
{
    addMembers();
    createBvi();

    expectTrap();
}

TEST_F(SwitchVppDhcpTrap, SetAttributeSwitchesBetweenTrapAndCopyOnExistingMembersAndBvi)
{
    auto trap = createTrap(SAI_PACKET_ACTION_TRAP);
    addMembers();
    createBvi();
    expectTrap();

    setTrapAction(trap, SAI_PACKET_ACTION_COPY);
    expectCopy();

    setTrapAction(trap, SAI_PACKET_ACTION_TRAP);
    expectTrap();

    setTrapAction(trap, SAI_PACKET_ACTION_COPY);
    expectCopy();
}

TEST_F(SwitchVppDhcpTrap, OtherActionsKeepTheTrapBehavior)
{
    auto trap = createTrap(SAI_PACKET_ACTION_COPY);
    addMembers();
    createBvi();

    setTrapAction(trap, SAI_PACKET_ACTION_FORWARD);
    expectTrap();

    setTrapAction(trap, SAI_PACKET_ACTION_DROP);
    expectTrap();
}

TEST_F(SwitchVppDhcpTrap, RemovingTheCopyTrapRevertsToTrap)
{
    auto trap = createTrap(SAI_PACKET_ACTION_COPY);
    addMembers();
    createBvi();
    expectCopy();

    removeTrap(trap);
    expectTrap();
}

TEST_F(SwitchVppDhcpTrap, ReplacementCopyTrapWinsOnlyOnceTheOldTrapIsGone)
{
    auto oldTrap = createTrap(SAI_PACKET_ACTION_TRAP);
    addMembers();
    createBvi();

    createTrap(SAI_PACKET_ACTION_COPY);
    expectTrap();

    removeTrap(oldTrap);
    expectCopy();
}

TEST_F(SwitchVppDhcpTrap, CopyLeavesTheOtherPuntsAlone)
{
    createTrap(SAI_PACKET_ACTION_COPY);
    addMembers();

    // broadcast ARP is still punted on both members (proxy ARP VLAN), through the
    // tagged member's chain that also holds the DHCP table
    EXPECT_EQ(memberPunt(), onMember("bobm0", false, arpOnWire(false)));
    EXPECT_EQ(tagFixup(), onMember("bobm1.100", true, arpOnWire(true)));

    // and LLDP on the untagged member
    std::vector<uint8_t> lldp = {0x01, 0x80, 0xc2, 0, 0, 0x0e, 0x02, 0, 0, 0, 0, 0x01, 0x88, 0xcc};
    lldp.resize(64, 0);
    EXPECT_EQ(memberPunt(), onMember("bobm0", false, lldp));
}

/*
 * The local 255.255.255.255/32 route is installed only while the DHCP trap
 * action is COPY: under TRAP (or without a DHCP trap) a client broadcast is
 * punted at the VLAN member and routed ports keep VPP's default drop on
 * 255.255.255.255/32, while 255.255.255.255 stays in the IP2ME address set once
 * any VRF table is set up.
 */
TEST_F(SwitchVppDhcpTrap, TheBviTableReceivesLimitedBroadcastsOnlyUnderCopy)
{
    auto trap = createTrap(SAI_PACKET_ACTION_TRAP);
    addMembers();

    EXPECT_FALSE(receivesLimitedBroadcast(0));
    EXPECT_FALSE(policesLimitedBroadcast());

    createBvi();

    expectTrap();
    EXPECT_TRUE(policesLimitedBroadcast());

    setTrapAction(trap, SAI_PACKET_ACTION_COPY);
    expectCopy();

    // the route stays with the table, not the BVI, while COPY is active
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->vpp_delete_bvi_interface(m_rif));

    EXPECT_TRUE(receivesLimitedBroadcast(0));
    EXPECT_TRUE(policesLimitedBroadcast());

    setTrapAction(trap, SAI_PACKET_ACTION_TRAP);

    EXPECT_FALSE(receivesLimitedBroadcast(0));
    EXPECT_TRUE(policesLimitedBroadcast());
}

TEST_F(SwitchVppDhcpTrap, AVrfTableReceivesLimitedBroadcastsUntilItIsRemoved)
{
    auto vrf = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, m_switchId);

    // without a COPY DHCP trap, a VRF table keeps VPP's default drop while
    // 255.255.255.255 is registered in the IP2ME address set
    ASSERT_EQ(0, m_sw->vpp_add_ip_vrf(vrf, VRF_TABLE));
    EXPECT_FALSE(receivesLimitedBroadcast(VRF_TABLE));
    EXPECT_TRUE(policesLimitedBroadcast());
    ASSERT_EQ(0, m_sw->vpp_del_ip_vrf(vrf));

    createTrap(SAI_PACKET_ACTION_COPY);

    ASSERT_EQ(0, m_sw->vpp_add_ip_vrf(vrf, VRF_TABLE));

    EXPECT_TRUE(receivesLimitedBroadcast(VRF_TABLE));
    EXPECT_FALSE(receivesLimitedBroadcast(0));
    EXPECT_TRUE(policesLimitedBroadcast());

    // a second router interface in the VRF finds it there and adds nothing
    vppMock().calls.clear();

    ASSERT_EQ(0, m_sw->vpp_add_ip_vrf(vrf, VRF_TABLE));

    EXPECT_TRUE(vppMock().calls.empty());

    // the route goes before the table
    ASSERT_EQ(0, m_sw->vpp_del_ip_vrf(vrf));

    ASSERT_EQ(2u, vppMock().calls.size());
    EXPECT_EQ("ip_route_add_del", vppMock().calls[0].api);
    EXPECT_EQ("255.255.255.255/32", vppMock().calls[0].name);
    EXPECT_EQ(VRF_TABLE, vppMock().calls[0].id);
    EXPECT_FALSE(vppMock().calls[0].flag);
    EXPECT_EQ("ip_vrf_del", vppMock().calls[1].api);
    EXPECT_FALSE(receivesLimitedBroadcast(VRF_TABLE));

    // the address set is switch-wide and stays once added
    EXPECT_TRUE(policesLimitedBroadcast());
}

/*
 * Table 0 outlives every VRF object, so removing one leaves its route in
 * place while COPY is active.
 */
TEST_F(SwitchVppDhcpTrap, TheDefaultTableKeepsItsRouteWhenAVrfObjectGoesAway)
{
    createTrap(SAI_PACKET_ACTION_COPY);

    ASSERT_EQ(0, m_sw->vpp_add_ip_vrf(m_defaultVr, 0));

    EXPECT_TRUE(receivesLimitedBroadcast(0));

    ASSERT_EQ(0, m_sw->vpp_del_ip_vrf(m_defaultVr));

    EXPECT_TRUE(receivesLimitedBroadcast(0));
}

/*
 * A LAG in a VLAN: SONiC's PortChannel24 is VPP's BondEthernet24, an untagged
 * member of bridge domain 905, behind a bridge port whose PORT_ID is the LAG.
 * A MAC VPP learns on the bond reaches SAI through the L2 MAC event queue that
 * processFdbEntriesForAging() drains.
 *
 * The unit tests are built with access control checking disabled, so the
 * fixture reaches the protected and private SwitchVpp members directly.
 */
class SwitchVppLagFdb : public ::testing::Test
{
    protected:

        void SetUp() override
        {
            auto sc = std::make_shared<SwitchConfig>(0, "");

            sc->m_saiSwitchType = SAI_SWITCH_TYPE_NPU;
            sc->m_switchType = SAI_VS_SWITCH_TYPE_VPP;
            sc->m_bootType = SAI_VS_BOOT_TYPE_COLD;
            vppMock().reset();
            sc->m_useTapDevice = false;
            sc->m_profileMap[SAI_KEY_VS_PORT_CONFIG_FILE] = "/dev/null";
            sc->m_laneMap = LaneMap::getDefaultLaneMap(0);
            sc->m_eventQueue = std::make_shared<EventQueue>(std::make_shared<Signal>());

            auto scc = std::make_shared<SwitchConfigContainer>();

            scc->insert(sc);

            m_mgr = std::make_shared<RealObjectIdManager>(0, scc);
            m_sw = std::make_shared<SwitchVpp>(m_switchId, m_mgr, sc);
            sc->m_useTapDevice = true;

            m_vlan = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_VLAN, m_switchId);

            sai_attribute_t vattr;

            vattr.id = SAI_VLAN_ATTR_VLAN_ID;
            vattr.value.u16 = VLAN_ID;

            ASSERT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create_internal(SAI_OBJECT_TYPE_VLAN, sai_serialize_object_id(m_vlan), m_switchId, 1, &vattr));

            m_lag = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_LAG, m_switchId);

            sai_attribute_t lattr;

            lattr.id = SAI_LAG_ATTR_PORT_VLAN_ID;
            lattr.value.u16 = VLAN_ID;

            ASSERT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create_internal(SAI_OBJECT_TYPE_LAG, sai_serialize_object_id(m_lag), m_switchId, 1, &lattr));

            // what vpp_create_lag() and vpp_create_vlan_member() record
            ASSERT_NE(nullptr, m_sw->m_ifaceRegistry.addLag(BOND_ID, BOND_SW_IF_INDEX, m_lag));
            ASSERT_TRUE(m_sw->m_ifaceRegistry.setBdId(BOND_HWIF, VLAN_ID));

            m_bridgePort = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_BRIDGE_PORT, m_switchId);

            sai_attribute_t battrs[2];

            battrs[0].id = SAI_BRIDGE_PORT_ATTR_TYPE;
            battrs[0].value.s32 = SAI_BRIDGE_PORT_TYPE_PORT;
            battrs[1].id = SAI_BRIDGE_PORT_ATTR_PORT_ID;
            battrs[1].value.oid = m_lag;

            ASSERT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create_internal(SAI_OBJECT_TYPE_BRIDGE_PORT, sai_serialize_object_id(m_bridgePort), m_switchId, 2, battrs));

            vppMock().calls.clear();
        }

        void TearDown() override
        {
            m_sw->m_switchConfig->m_useTapDevice = false;
            m_sw.reset();
        }

        // VPP reports a MAC event on the bond, and the FDB aging thread drains it
        void vppMacEvent(
                uint8_t action)
        {
            SWSS_LOG_ENTER();

            vpp_mac_event_t ev;

            memcpy(ev.mac, MAC, sizeof(ev.mac));
            ev.sw_if_index = BOND_SW_IF_INDEX;
            ev.action = action;

            SwitchVpp::staticMacEventCb(&ev, 1, m_sw.get());

            m_sw->processFdbEntriesForAging();
        }

        // the SAI FDB entry of MAC in the VLAN, as ASIC_DB sees it, or null
        std::shared_ptr<SaiAttrWrap> fdbBridgePort()
        {
            SWSS_LOG_ENTER();

            auto& fdbs = m_sw->m_objectHash.at(SAI_OBJECT_TYPE_FDB_ENTRY);

            for (auto& kv: fdbs)
            {
                sai_fdb_entry_t fdb;

                sai_deserialize_fdb_entry(kv.first, fdb);

                if (fdb.bv_id == m_vlan && memcmp(fdb.mac_address, MAC, sizeof(sai_mac_t)) == 0)
                {
                    return kv.second.at("SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID");
                }
            }

            return nullptr;
        }

        /*
         * What VirtualSwitchSaiInterface::flushFdbEntries() does around the VPP
         * switch: drop the matching SAI entries and their m_fdb_info_set
         * records (the FLUSHED notification then clears ASIC_DB and STATE_DB),
         * and hand the flush to vpp_fdbentry_flush(). Its result is not
         * checked there, so neither can a failure undo the SAI side.
         */
        sai_status_t flushBridgePort()
        {
            SWSS_LOG_ENTER();

            m_sw->m_objectHash.at(SAI_OBJECT_TYPE_FDB_ENTRY).clear();
            m_sw->m_fdb_info_set.clear();

            sai_attribute_t attrs[2];

            attrs[0].id = SAI_FDB_FLUSH_ATTR_BRIDGE_PORT_ID;
            attrs[0].value.oid = m_bridgePort;
            attrs[1].id = SAI_FDB_FLUSH_ATTR_ENTRY_TYPE;
            attrs[1].value.s32 = SAI_FDB_FLUSH_ENTRY_TYPE_DYNAMIC;

            return m_sw->vpp_fdbentry_flush(m_switchId, 2, attrs);
        }

        std::string fdbSid()
        {
            SWSS_LOG_ENTER();

            sai_fdb_entry_t fdb;

            memset(&fdb, 0, sizeof(fdb));
            fdb.switch_id = m_switchId;
            fdb.bv_id = m_vlan;
            memcpy(fdb.mac_address, MAC, sizeof(sai_mac_t));

            return sai_serialize_fdb_entry(fdb);
        }

        static constexpr uint16_t VLAN_ID = 905;
        static constexpr uint32_t BOND_ID = 24;
        static constexpr uint32_t BOND_SW_IF_INDEX = 124;
        static constexpr const char *BOND_HWIF = "BondEthernet24";
        static constexpr uint8_t MAC[6] = { 0x00, 0x50, 0x56, 0xac, 0xb2, 0x02 };

        const sai_object_id_t m_switchId = 0x2100000000;

        std::shared_ptr<RealObjectIdManager> m_mgr;

        std::shared_ptr<SwitchVpp> m_sw;

        sai_object_id_t m_vlan = SAI_NULL_OBJECT_ID;
        sai_object_id_t m_lag = SAI_NULL_OBJECT_ID;
        sai_object_id_t m_bridgePort = SAI_NULL_OBJECT_ID;
};

constexpr uint16_t SwitchVppLagFdb::VLAN_ID;
constexpr uint32_t SwitchVppLagFdb::BOND_ID;
constexpr uint32_t SwitchVppLagFdb::BOND_SW_IF_INDEX;
constexpr const char *SwitchVppLagFdb::BOND_HWIF;
constexpr uint8_t SwitchVppLagFdb::MAC[6];

/*
 * EVPN on a VLAN: a P2P VXLAN tunnel to each remote VTEP that advertised the
 * VLAN's VNI, created by orchagent, a tunnel bridge port for each,
 * and the remote MAC addresses as FDB entries on those bridge ports. The
 * fixture's LAG in the same VLAN is the local side.
 */
class SwitchVppEvpnRemoteMac : public SwitchVppLagFdb
{
    protected:

        void SetUp() override
        {
            SwitchVppLagFdb::SetUp();

            if (HasFatalFailure())
            {
                return;
            }

            m_mapper = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_TUNNEL_MAP, m_switchId);

            sai_attribute_t mattr;

            mattr.id = SAI_TUNNEL_MAP_ATTR_TYPE;
            mattr.value.s32 = SAI_TUNNEL_MAP_TYPE_VNI_TO_VLAN_ID;

            ASSERT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create_internal(SAI_OBJECT_TYPE_TUNNEL_MAP, sai_serialize_object_id(m_mapper), m_switchId, 1, &mattr));

            vppMock().calls.clear();
            g_nextSwIfIndex = 100;
            g_l2fibByIndexDelResult = 0;
            g_vxlanTunnelDst.clear();
        }

        void addMapEntry()
        {
            SWSS_LOG_ENTER();

            m_mapEntry = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_TUNNEL_MAP_ENTRY, m_switchId);

            sai_attribute_t eattrs[4];

            eattrs[0].id = SAI_TUNNEL_MAP_ENTRY_ATTR_TUNNEL_MAP_TYPE;
            eattrs[0].value.s32 = SAI_TUNNEL_MAP_TYPE_VNI_TO_VLAN_ID;
            eattrs[1].id = SAI_TUNNEL_MAP_ENTRY_ATTR_TUNNEL_MAP;
            eattrs[1].value.oid = m_mapper;
            eattrs[2].id = SAI_TUNNEL_MAP_ENTRY_ATTR_VNI_ID_KEY;
            eattrs[2].value.u32 = VNI;
            eattrs[3].id = SAI_TUNNEL_MAP_ENTRY_ATTR_VLAN_ID_VALUE;
            eattrs[3].value.u16 = VLAN_ID;

            ASSERT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create(SAI_OBJECT_TYPE_TUNNEL_MAP_ENTRY, sai_serialize_object_id(m_mapEntry), m_switchId, 4, eattrs));
        }

        static sai_ip_address_t ipv4(
                const char *addr)
        {
            SWSS_LOG_ENTER();

            sai_ip_address_t ip;

            memset(&ip, 0, sizeof(ip));
            ip.addr_family = SAI_IP_ADDR_FAMILY_IPV4;

            EXPECT_EQ(1, inet_pton(AF_INET, addr, &ip.addr.ip4));

            return ip;
        }

        // orchagent's P2P tunnel to a remote VTEP, and its tunnel bridge port
        sai_object_id_t addRemoteVtep(
                const char *dst,
                sai_object_id_t *bridgePort = nullptr)
        {
            SWSS_LOG_ENTER();

            auto tunnel = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_TUNNEL, m_switchId);

            sai_attribute_t tattrs[4];

            tattrs[0].id = SAI_TUNNEL_ATTR_TYPE;
            tattrs[0].value.s32 = SAI_TUNNEL_TYPE_VXLAN;
            tattrs[1].id = SAI_TUNNEL_ATTR_ENCAP_SRC_IP;
            tattrs[1].value.ipaddr = ipv4(LOCAL_VTEP);
            tattrs[2].id = SAI_TUNNEL_ATTR_ENCAP_DST_IP;
            tattrs[2].value.ipaddr = ipv4(dst);
            tattrs[3].id = SAI_TUNNEL_ATTR_DECAP_MAPPERS;
            tattrs[3].value.objlist.count = 1;
            tattrs[3].value.objlist.list = &m_mapper;

            EXPECT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create(SAI_OBJECT_TYPE_TUNNEL, sai_serialize_object_id(tunnel), m_switchId, 4, tattrs));

            auto bp = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_BRIDGE_PORT, m_switchId);

            sai_attribute_t battrs[2];

            battrs[0].id = SAI_BRIDGE_PORT_ATTR_TYPE;
            battrs[0].value.s32 = SAI_BRIDGE_PORT_TYPE_TUNNEL;
            battrs[1].id = SAI_BRIDGE_PORT_ATTR_TUNNEL_ID;
            battrs[1].value.oid = tunnel;

            EXPECT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create_internal(SAI_OBJECT_TYPE_BRIDGE_PORT, sai_serialize_object_id(bp), m_switchId, 2, battrs));

            if (bridgePort)
            {
                *bridgePort = bp;
            }

            return tunnel;
        }

        // the sw_if_index of the VXLAN tunnel the stub created to a VTEP, or ~0
        static uint32_t tunnelTo(
                const char *dst)
        {
            SWSS_LOG_ENTER();

            for (auto& kv: g_vxlanTunnelDst)
            {
                if (kv.second == dst)
                {
                    return kv.first;
                }
            }

            return ~0u;
        }

        static std::string onTunnel(
                uint32_t sw_if_index)
        {
            SWSS_LOG_ENTER();

            return std::to_string(sw_if_index) + " " + macStr(REMOTE_MAC);
        }

        std::string remoteSid()
        {
            SWSS_LOG_ENTER();

            sai_fdb_entry_t fdb;

            memset(&fdb, 0, sizeof(fdb));
            fdb.switch_id = m_switchId;
            fdb.bv_id = m_vlan;
            memcpy(fdb.mac_address, REMOTE_MAC, sizeof(sai_mac_t));

            return sai_serialize_fdb_entry(fdb);
        }

        /*
         * What orchagent creates for a type-2 route from a remote VTEP:
         * STATIC, on the VTEP's tunnel bridge port, with the VTEP as the
         * endpoint, and allowed to move unless the route was sticky.
         */
        sai_status_t addRemoteMac(
                sai_object_id_t bridgePort,
                const char *endpoint,
                bool allowMove = true)
        {
            SWSS_LOG_ENTER();

            sai_attribute_t attrs[4];
            uint32_t count = 3;

            attrs[0].id = SAI_FDB_ENTRY_ATTR_TYPE;
            attrs[0].value.s32 = SAI_FDB_ENTRY_TYPE_STATIC;
            attrs[1].id = SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID;
            attrs[1].value.oid = bridgePort;
            attrs[2].id = SAI_FDB_ENTRY_ATTR_ENDPOINT_IP;
            attrs[2].value.ipaddr = ipv4(endpoint);

            if (allowMove)
            {
                attrs[3].id = SAI_FDB_ENTRY_ATTR_ALLOW_MAC_MOVE;
                attrs[3].value.booldata = true;
                count = 4;
            }

            return m_sw->create(SAI_OBJECT_TYPE_FDB_ENTRY, remoteSid(), m_switchId, count, attrs);
        }

        sai_status_t setRemoteMac(
                sai_attr_id_t id,
                sai_object_id_t bridgePort,
                const char *endpoint = nullptr)
        {
            SWSS_LOG_ENTER();

            sai_attribute_t attr;

            memset(&attr, 0, sizeof(attr));
            attr.id = id;

            if (id == SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID)
            {
                attr.value.oid = bridgePort;
            }
            else
            {
                attr.value.ipaddr = ipv4(endpoint);
            }

            return m_sw->set(SAI_OBJECT_TYPE_FDB_ENTRY, remoteSid(), &attr);
        }

        static constexpr uint32_t VNI = 1905;
        static constexpr const char *LOCAL_VTEP = "10.0.0.1";
        static constexpr const char *VTEP_A = "10.0.0.2";
        static constexpr const char *VTEP_B = "10.0.0.3";
        static constexpr const char *VTEP_C = "10.0.0.4";
        static constexpr uint8_t REMOTE_MAC[6] = { 0x00, 0x50, 0x56, 0xac, 0xb0, 0x02 };

        sai_object_id_t m_mapper = SAI_NULL_OBJECT_ID;
        sai_object_id_t m_mapEntry = SAI_NULL_OBJECT_ID;
};

constexpr uint32_t SwitchVppEvpnRemoteMac::VNI;
constexpr const char *SwitchVppEvpnRemoteMac::LOCAL_VTEP;
constexpr const char *SwitchVppEvpnRemoteMac::VTEP_A;
constexpr const char *SwitchVppEvpnRemoteMac::VTEP_B;
constexpr const char *SwitchVppEvpnRemoteMac::VTEP_C;
constexpr uint8_t SwitchVppEvpnRemoteMac::REMOTE_MAC[6];

/*
 * VPP tells VXLAN tunnels apart by source, destination and VNI. Every remote
 * VTEP of a VNI needs its own, or VPP drops what the others send and the
 * switch can send to only one of them.
 */
TEST_F(SwitchVppEvpnRemoteMac, EveryRemoteVtepOfAVniGetsItsOwnTunnel)
{
    addMapEntry();
    addRemoteVtep(VTEP_A);
    addRemoteVtep(VTEP_B);
    addRemoteVtep(VTEP_C);

    auto adds = vppCallsTo("vpp_vxlan_tunnel_add_del");

    ASSERT_EQ(3u, adds.size());

    for (auto& add: adds)
    {
        EXPECT_TRUE(add.flag);
        EXPECT_EQ(VNI, add.id);
    }

    EXPECT_NE(~0u, tunnelTo(VTEP_A));
    EXPECT_NE(~0u, tunnelTo(VTEP_B));
    EXPECT_NE(~0u, tunnelTo(VTEP_C));
}

/*
 * Each VTEP floods BUM traffic to all of its peers itself (head-end
 * replication), so a frame from one tunnel must not go out the others: the
 * tunnels share a split horizon group other than the local ports' 0. MAC
 * addresses behind a tunnel come from EVPN, not from data-plane learning.
 */
TEST_F(SwitchVppEvpnRemoteMac, TunnelsJoinTheVlanInSplitHorizonGroupOneWithLearningOff)
{
    addMapEntry();
    addRemoteVtep(VTEP_A);
    addRemoteVtep(VTEP_B);

    for (auto dst: {VTEP_A, VTEP_B})
    {
        auto swif = std::to_string(tunnelTo(dst));

        int join = vppCallIndex("set_sw_interface_l2_bridge_shg_by_index", swif);

        ASSERT_NE(-1, join) << dst;
        EXPECT_EQ((uint32_t)VLAN_ID, vppMock().calls[join].id);
        EXPECT_EQ(1u, vppMock().calls[join].sub_id) << "split horizon group of the tunnel to " << dst;
        EXPECT_TRUE(vppMock().calls[join].flag);

        int learn = vppCallIndex("set_l2_interface_flags_by_index", swif);

        ASSERT_NE(-1, learn) << dst;
        EXPECT_EQ((uint32_t)VPP_BD_FLAG_LEARN, vppMock().calls[learn].id);
        EXPECT_FALSE(vppMock().calls[learn].flag);
        // joining the bridge domain turns learning on, so it goes off after
        EXPECT_LT(join, learn);
    }
}

TEST_F(SwitchVppEvpnRemoteMac, RemoteMacIsProgrammedOnTheTunnelToItsVtep)
{
    addMapEntry();

    sai_object_id_t bpA, bpB;

    addRemoteVtep(VTEP_A, &bpA);
    addRemoteVtep(VTEP_B, &bpB);

    vppMock().calls.clear();

    ASSERT_EQ(SAI_STATUS_SUCCESS, addRemoteMac(bpB, VTEP_B));

    auto adds = vppCallsTo("l2fib_add_del_by_index");

    ASSERT_EQ(1u, adds.size());
    EXPECT_EQ(onTunnel(tunnelTo(VTEP_B)), adds[0].name);
    EXPECT_EQ((uint32_t)VLAN_ID, adds[0].id);
    EXPECT_TRUE(adds[0].flag);
    // not static: a host that moves here is learned on the local port
    EXPECT_EQ(0u, adds[0].sub_id);

    vppMock().calls.clear();

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->remove(SAI_OBJECT_TYPE_FDB_ENTRY, remoteSid()));

    auto dels = vppCallsTo("l2fib_add_del_by_index");

    ASSERT_EQ(1u, dels.size());
    EXPECT_EQ(onTunnel(tunnelTo(VTEP_B)), dels[0].name);
    EXPECT_EQ((uint32_t)VLAN_ID, dels[0].id);
    EXPECT_FALSE(dels[0].flag);
}

/* a sticky MAC address (no move allowed) stays static in VPP */
TEST_F(SwitchVppEvpnRemoteMac, StickyRemoteMacIsStatic)
{
    addMapEntry();

    sai_object_id_t bpA;

    addRemoteVtep(VTEP_A, &bpA);

    vppMock().calls.clear();

    ASSERT_EQ(SAI_STATUS_SUCCESS, addRemoteMac(bpA, VTEP_A, false));

    auto adds = vppCallsTo("l2fib_add_del_by_index");

    ASSERT_EQ(1u, adds.size());
    EXPECT_EQ(onTunnel(tunnelTo(VTEP_A)), adds[0].name);
    EXPECT_EQ(1u, adds[0].sub_id);
}

/* SAI_FDB_ENTRY_ATTR_ENDPOINT_IP picks the VTEP, not the bridge port's tunnel */
TEST_F(SwitchVppEvpnRemoteMac, EndpointIpPicksTheTunnel)
{
    addMapEntry();

    sai_object_id_t bpA;

    addRemoteVtep(VTEP_A, &bpA);
    addRemoteVtep(VTEP_B);

    vppMock().calls.clear();

    ASSERT_EQ(SAI_STATUS_SUCCESS, addRemoteMac(bpA, VTEP_B));

    auto adds = vppCallsTo("l2fib_add_del_by_index");

    ASSERT_EQ(1u, adds.size());
    EXPECT_EQ(onTunnel(tunnelTo(VTEP_B)), adds[0].name);
}

/*
 * Every VTEP with a gateway on the VLAN advertises the gateway MAC, an anycast
 * one included. It stays on the local BVI: VPP routes a frame to it only
 * through the BVI's L2FIB entry, which an entry on a tunnel would replace.
 */
TEST_F(SwitchVppEvpnRemoteMac, GatewayMacOfTheVlanIsNotPointedAtATunnel)
{
    addMapEntry();

    sai_object_id_t bpA;

    addRemoteVtep(VTEP_A, &bpA);

    auto rif = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_ROUTER_INTERFACE, m_switchId);

    sai_attribute_t rattrs[4];

    rattrs[0].id = SAI_ROUTER_INTERFACE_ATTR_TYPE;
    rattrs[0].value.s32 = SAI_ROUTER_INTERFACE_TYPE_VLAN;
    rattrs[1].id = SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID;
    rattrs[1].value.oid = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, m_switchId);
    rattrs[2].id = SAI_ROUTER_INTERFACE_ATTR_VLAN_ID;
    rattrs[2].value.oid = m_vlan;
    rattrs[3].id = SAI_ROUTER_INTERFACE_ATTR_SRC_MAC_ADDRESS;
    memcpy(rattrs[3].value.mac, REMOTE_MAC, sizeof(sai_mac_t));

    // the SAI object is enough; creating it through create() would build the BVI
    ASSERT_EQ(SAI_STATUS_SUCCESS,
            m_sw->create_internal(SAI_OBJECT_TYPE_ROUTER_INTERFACE, sai_serialize_object_id(rif), m_switchId, 4, rattrs));

    vppMock().calls.clear();

    ASSERT_EQ(SAI_STATUS_SUCCESS, addRemoteMac(bpA, VTEP_A));

    EXPECT_TRUE(vppCallsTo("l2fib_add_del_by_index").empty());
}

/*
 * orchagent moves a remote MAC address to another VTEP by setting the bridge
 * port, then the endpoint, one at a time. VPP has to end up with it on the
 * new VTEP's tunnel, and replacing the entry in place leaves no gap in which
 * VPP drops frames to it.
 */
TEST_F(SwitchVppEvpnRemoteMac, RemoteToRemoteMoveRepointsTheMac)
{
    addMapEntry();

    sai_object_id_t bpA, bpB;

    addRemoteVtep(VTEP_A, &bpA);
    addRemoteVtep(VTEP_B, &bpB);

    ASSERT_EQ(SAI_STATUS_SUCCESS, addRemoteMac(bpA, VTEP_A));

    vppMock().calls.clear();

    ASSERT_EQ(SAI_STATUS_SUCCESS, setRemoteMac(SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID, bpB));
    ASSERT_EQ(SAI_STATUS_SUCCESS, setRemoteMac(SAI_FDB_ENTRY_ATTR_ENDPOINT_IP, SAI_NULL_OBJECT_ID, VTEP_B));

    auto calls = vppCallsTo("l2fib_add_del_by_index");

    ASSERT_EQ(1u, calls.size());
    EXPECT_EQ(onTunnel(tunnelTo(VTEP_B)), calls[0].name);
    EXPECT_TRUE(calls[0].flag);

    vppMock().calls.clear();

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->remove(SAI_OBJECT_TYPE_FDB_ENTRY, remoteSid()));

    auto dels = vppCallsTo("l2fib_add_del_by_index");

    ASSERT_EQ(1u, dels.size());
    EXPECT_EQ(onTunnel(tunnelTo(VTEP_B)), dels[0].name);
    EXPECT_FALSE(dels[0].flag);
}

/*
 * A host that moves from a remote VTEP to a local port: VPP moves the entry
 * itself, since it is not static, and reports the move. SONiC has the MAC
 * address already, as a remote one, so that has to reach orchagent as a move
 * to the local bridge port, which it turns into a local MAC.
 */
TEST_F(SwitchVppEvpnRemoteMac, RemoteMacThatMovesToALocalPortIsReportedThere)
{
    addMapEntry();

    sai_object_id_t bpA;

    addRemoteVtep(VTEP_A, &bpA);

    ASSERT_EQ(SAI_STATUS_SUCCESS, addRemoteMac(bpA, VTEP_A));

    vpp_mac_event_t ev;

    memcpy(ev.mac, REMOTE_MAC, sizeof(ev.mac));
    ev.sw_if_index = BOND_SW_IF_INDEX;
    ev.action = VPP_MAC_ACTION_MOVE;

    SwitchVpp::staticMacEventCb(&ev, 1, m_sw.get());

    m_sw->processFdbEntriesForAging();

    auto& entry = m_sw->m_objectHash.at(SAI_OBJECT_TYPE_FDB_ENTRY).at(remoteSid());

    EXPECT_EQ(m_bridgePort, entry.at("SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID")->getAttr()->value.oid);
    EXPECT_EQ(SAI_FDB_ENTRY_TYPE_DYNAMIC, entry.at("SAI_FDB_ENTRY_ATTR_TYPE")->getAttr()->value.s32);
}

/*
 * orchagent removes a VTEP's P2P tunnel when the VTEP leaves. Its VPP
 * tunnels go with it, and the tunnels to every other VTEP stay.
 */
TEST_F(SwitchVppEvpnRemoteMac, RemovingTheSaiTunnelRemovesItsL2Tunnels)
{
    addMapEntry();

    sai_object_id_t bpA;

    auto tunnelA = addRemoteVtep(VTEP_A, &bpA);
    addRemoteVtep(VTEP_B);

    ASSERT_EQ(SAI_STATUS_SUCCESS, addRemoteMac(bpA, VTEP_A));

    uint32_t swifA = tunnelTo(VTEP_A);

    vppMock().calls.clear();

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->remove(SAI_OBJECT_TYPE_TUNNEL, sai_serialize_object_id(tunnelA)));

    auto dels = vppCallsTo("vpp_vxlan_tunnel_add_del");

    ASSERT_EQ(1u, dels.size());
    EXPECT_FALSE(dels[0].flag);
    EXPECT_EQ(VNI, dels[0].id);

    int leave = vppCallIndex("set_sw_interface_l2_bridge_shg_by_index", std::to_string(swifA));

    ASSERT_NE(-1, leave);
    EXPECT_FALSE(vppMock().calls[leave].flag);

    // VPP keeps a provisioned L2FIB entry after its interface is gone
    auto macDels = vppCallsTo("l2fib_add_del_by_index");

    ASSERT_EQ(1u, macDels.size());
    EXPECT_EQ(onTunnel(swifA), macDels[0].name);
    EXPECT_FALSE(macDels[0].flag);
}

/*
 * The VNI-to-VLAN map entry can come after the tunnels (and their remote MAC
 * addresses). Every VTEP gets its tunnel then, and the MAC addresses waiting
 * for one are programmed on it.
 */
TEST_F(SwitchVppEvpnRemoteMac, LateMapEntryCreatesEveryTunnelAndProgramsWaitingMacs)
{
    sai_object_id_t bpB;

    addRemoteVtep(VTEP_A);
    addRemoteVtep(VTEP_B, &bpB);

    ASSERT_EQ(SAI_STATUS_SUCCESS, addRemoteMac(bpB, VTEP_B));

    EXPECT_TRUE(vppCallsTo("vpp_vxlan_tunnel_add_del").empty());
    EXPECT_TRUE(vppCallsTo("l2fib_add_del_by_index").empty());

    addMapEntry();

    EXPECT_EQ(2u, vppCallsTo("vpp_vxlan_tunnel_add_del").size());

    auto adds = vppCallsTo("l2fib_add_del_by_index");

    ASSERT_EQ(1u, adds.size());
    EXPECT_EQ(onTunnel(tunnelTo(VTEP_B)), adds[0].name);
    EXPECT_TRUE(adds[0].flag);
}

TEST_F(SwitchVppEvpnRemoteMac, RemovingTheMapEntryRemovesTheTunnelsOfItsVni)
{
    addMapEntry();
    addRemoteVtep(VTEP_A);
    addRemoteVtep(VTEP_B);

    vppMock().calls.clear();

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->remove(SAI_OBJECT_TYPE_TUNNEL_MAP_ENTRY, sai_serialize_object_id(m_mapEntry)));

    auto dels = vppCallsTo("vpp_vxlan_tunnel_add_del");

    ASSERT_EQ(2u, dels.size());

    for (auto& del: dels)
    {
        EXPECT_FALSE(del.flag);
        EXPECT_EQ(VNI, del.id);
    }
}

/*
 * L2 tunnels do not learn, so a flush of a tunnel bridge port has nothing to
 * take out of VPP. It must not flush the MAC addresses learned on local ports.
 */
TEST_F(SwitchVppEvpnRemoteMac, FlushOfATunnelBridgePortLeavesLocalMacsAlone)
{
    addMapEntry();

    sai_object_id_t bpA;

    addRemoteVtep(VTEP_A, &bpA);

    vppMacEvent(VPP_MAC_ACTION_ADD);

    ASSERT_NE(nullptr, fdbBridgePort());

    vppMock().calls.clear();

    sai_attribute_t attrs[2];

    attrs[0].id = SAI_FDB_FLUSH_ATTR_BRIDGE_PORT_ID;
    attrs[0].value.oid = bpA;
    attrs[1].id = SAI_FDB_FLUSH_ATTR_ENTRY_TYPE;
    attrs[1].value.s32 = SAI_FDB_FLUSH_ENTRY_TYPE_DYNAMIC;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->vpp_fdbentry_flush(m_switchId, 2, attrs));

    EXPECT_TRUE(vppCallsTo("l2fib_flush_all").empty());
    EXPECT_TRUE(vppCallsTo("l2fib_add_del").empty());
    EXPECT_FALSE(m_sw->m_vpp_fdb_entries.empty());
}

/* a flush that dropped a remote MAC address on the SAI side deletes it from VPP */
TEST_F(SwitchVppEvpnRemoteMac, FlushedRemoteMacIsDeletedFromVpp)
{
    addMapEntry();

    sai_object_id_t bpA;

    addRemoteVtep(VTEP_A, &bpA);

    ASSERT_EQ(SAI_STATUS_SUCCESS, addRemoteMac(bpA, VTEP_A));

    // what VirtualSwitchSaiInterface::flushFdbEntries() does before calling in
    m_sw->m_objectHash.at(SAI_OBJECT_TYPE_FDB_ENTRY).erase(remoteSid());

    vppMock().calls.clear();

    sai_attribute_t attrs[2];

    attrs[0].id = SAI_FDB_FLUSH_ATTR_BRIDGE_PORT_ID;
    attrs[0].value.oid = bpA;
    attrs[1].id = SAI_FDB_FLUSH_ATTR_ENTRY_TYPE;
    attrs[1].value.s32 = SAI_FDB_FLUSH_ENTRY_TYPE_ALL;

    m_sw->vpp_fdbentry_flush(m_switchId, 2, attrs);

    auto dels = vppCallsTo("l2fib_add_del_by_index");

    ASSERT_EQ(1u, dels.size());
    EXPECT_EQ(onTunnel(tunnelTo(VTEP_A)), dels[0].name);
    EXPECT_FALSE(dels[0].flag);
}

TEST_F(SwitchVppEvpnRemoteMac, FailedRemoteMacDeleteKeepsTheEntryForRetry)
{
    addMapEntry();

    sai_object_id_t bpA;

    addRemoteVtep(VTEP_A, &bpA);

    ASSERT_EQ(SAI_STATUS_SUCCESS, addRemoteMac(bpA, VTEP_A));

    vppMock().calls.clear();
    g_l2fibByIndexDelResult = -1;

    EXPECT_EQ(SAI_STATUS_FAILURE, m_sw->remove(SAI_OBJECT_TYPE_FDB_ENTRY, remoteSid()));
    EXPECT_EQ(1u, m_sw->m_remote_fdb.count(remoteSid()));
    EXPECT_EQ(1u, m_sw->m_objectHash.at(SAI_OBJECT_TYPE_FDB_ENTRY).count(remoteSid()));

    vppMock().calls.clear();
    g_l2fibByIndexDelResult = 0;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->remove(SAI_OBJECT_TYPE_FDB_ENTRY, remoteSid()));
    EXPECT_EQ(0u, m_sw->m_remote_fdb.count(remoteSid()));

    auto dels = vppCallsTo("l2fib_add_del_by_index");

    ASSERT_EQ(1u, dels.size());
    EXPECT_EQ(onTunnel(tunnelTo(VTEP_A)), dels[0].name);
    EXPECT_FALSE(dels[0].flag);
}

TEST_F(SwitchVppEvpnRemoteMac, FailedTunnelTeardownKeepsTheLastOwnerForRetry)
{
    addMapEntry();

    sai_object_id_t bpA;

    auto tunnelA = addRemoteVtep(VTEP_A, &bpA);

    ASSERT_EQ(SAI_STATUS_SUCCESS, addRemoteMac(bpA, VTEP_A));

    uint32_t swifA = tunnelTo(VTEP_A);

    vppMock().calls.clear();
    g_l2fibByIndexDelResult = -1;

    EXPECT_EQ(SAI_STATUS_FAILURE, m_sw->remove(SAI_OBJECT_TYPE_TUNNEL, sai_serialize_object_id(tunnelA)));
    EXPECT_TRUE(vppCallsTo("vpp_vxlan_tunnel_add_del").empty());
    EXPECT_EQ(1u, m_sw->m_objectHash.at(SAI_OBJECT_TYPE_TUNNEL).count(sai_serialize_object_id(tunnelA)));

    vppMock().calls.clear();
    g_l2fibByIndexDelResult = 0;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->remove(SAI_OBJECT_TYPE_TUNNEL, sai_serialize_object_id(tunnelA)));

    auto dels = vppCallsTo("vpp_vxlan_tunnel_add_del");

    ASSERT_EQ(1u, dels.size());
    EXPECT_FALSE(dels[0].flag);
    EXPECT_EQ(VNI, dels[0].id);

    auto macDels = vppCallsTo("l2fib_add_del_by_index");

    ASSERT_EQ(1u, macDels.size());
    EXPECT_EQ(onTunnel(swifA), macDels[0].name);
    EXPECT_FALSE(macDels[0].flag);
}
