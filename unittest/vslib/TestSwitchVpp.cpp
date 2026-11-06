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
    /*
     * The classify tables, sessions and interface bindings SwitchVpp creates,
     * so a test can ask what VPP would do with a given frame.
     */
    struct ClassifyTable
    {
        uint32_t skip;
        uint32_t match;
        uint32_t next_table;
        std::vector<uint8_t> mask;
        std::map<std::vector<uint8_t>, uint32_t> sessions; // masked key -> hit next
    };

    std::map<uint32_t, ClassifyTable> g_tables;
    uint32_t g_nextTableIndex = 0;

    // interface -> l2 input tables {ip4, ip6, other}
    std::map<std::string, std::array<uint32_t, 3>> g_l2Tables;

    const uint32_t MISS = ~0u;

    std::vector<uint8_t> maskedKey(
            const ClassifyTable& t,
            const uint8_t *data,
            size_t len)
    {
        SWSS_LOG_ENTER();

        std::vector<uint8_t> key(t.match * 16, 0);

        for (size_t i = 0; i < key.size(); i++)
        {
            size_t off = t.skip * 16 + i;

            key[i] = (uint8_t)((off < len ? data[off] : 0) & t.mask[i]);
        }

        return key;
    }

    // Walks a classify chain the way VPP does: the first hit's next index, or MISS.
    uint32_t classify(
            uint32_t table,
            const std::vector<uint8_t>& frame)
    {
        SWSS_LOG_ENTER();

        while (table != ~0u)
        {
            auto it = g_tables.find(table);

            if (it == g_tables.end())
            {
                return MISS;
            }

            auto s = it->second.sessions.find(maskedKey(it->second, frame.data(), frame.size()));

            if (s != it->second.sessions.end())
            {
                return s->second;
            }

            table = it->second.next_table;
        }

        return MISS;
    }

    // IPv4 API routes VPP holds: (table, prefix) -> next hop type
    std::map<std::pair<uint32_t, std::string>, int> g_ip4Routes;

    // Active FIB next hop sw_if_index list per (table, prefix) programmed via ip_route_add_del_get_stats
    std::map<std::pair<uint32_t, std::string>, std::vector<uint32_t>> g_fibRoutePaths;
    uint32_t g_nextRouteStatsIndex = 1;

    // the IP2ME address set: the destinations the IP2ME policer meters on the punt path
    std::set<std::string> g_ip2meAddrs;

    uint32_t g_nextSwIfIndex = 100;
    int g_l2fibByIndexDelResult = 0;

    // destination address of each VXLAN tunnel the stub created, by sw_if_index
    std::map<uint32_t, std::string> g_vxlanTunnelDst;
    std::map<uint32_t, bool> g_vxlanTunnelDecapAny;

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

    std::vector<VppCall> vppCallsTo(
            const std::string& api,
            const std::string& name)
    {
        SWSS_LOG_ENTER();

        return vppMock().callsTo(api, name);
    }
}

extern "C" {

int __wrap_vpp_classify_table_create(uint32_t nbuckets, uint32_t memory_size,
                                     uint32_t skip_n_vectors, uint32_t match_n_vectors,
                                     uint32_t next_table_index, uint32_t miss_next_index,
                                     const uint8_t *mask, uint32_t mask_len,
                                     uint32_t *new_table_index)
{
    SWSS_LOG_ENTER();

    EXPECT_EQ(match_n_vectors * 16, mask_len);

    ClassifyTable t;

    t.skip = skip_n_vectors;
    t.match = match_n_vectors;
    t.next_table = next_table_index;
    t.mask.assign(mask, mask + mask_len);

    *new_table_index = g_nextTableIndex++;
    g_tables[*new_table_index] = t;
    return 0;
}

int __wrap_vpp_classify_table_delete(uint32_t table_index)
{
    SWSS_LOG_ENTER();

    g_tables.erase(table_index);
    return 0;
}

int __wrap_vpp_classify_session_add(uint32_t table_index, uint32_t hit_next_index,
                                    const uint8_t *match, uint32_t match_len,
                                    uint32_t opaque_index, int32_t advance, uint8_t action)
{
    SWSS_LOG_ENTER();

    vppMock().classifySessions.push_back({table_index, hit_next_index,
            std::vector<uint8_t>(match, match + match_len)});

    auto& t = g_tables.at(table_index);

    t.sessions[maskedKey(t, match, match_len)] = hit_next_index;
    return 0;
}

int __wrap_vpp_classify_session_del(uint32_t table_index, const uint8_t *match, uint32_t match_len)
{
    SWSS_LOG_ENTER();

    auto& t = g_tables.at(table_index);

    // VPP fails the delete of a session that is not there
    return t.sessions.erase(maskedKey(t, match, match_len)) == 1 ? 0 : -1;
}

int __wrap_vpp_classify_set_interface_l2_tables(const char *hwif_name, uint32_t ip4_table_index,
        uint32_t ip6_table_index, uint32_t other_table_index, bool is_input)
{
    SWSS_LOG_ENTER();

    g_l2Tables[hwif_name] = {ip4_table_index, ip6_table_index, other_table_index};
    return 0;
}

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

int __wrap_ip_route_add_del_get_stats(vpp_ip_route_t *route, bool is_add, uint32_t *stats_index)
{
    SWSS_LOG_ENTER();

    if (stats_index)
    {
        *stats_index = is_add ? g_nextRouteStatsIndex++ : UINT32_MAX;
    }

    if (route->prefix_addr.sa_family != AF_INET || route->nexthop_cnt < 1)
    {
        return 0;
    }

    if (route->vrf_id != 0 && vppMock().vrfTables.count(route->vrf_id) == 0)
    {
        return -1;
    }

    char addr[INET_ADDRSTRLEN];

    inet_ntop(AF_INET, &route->prefix_addr.addr.ip4.sin_addr, addr, sizeof(addr));

    std::string prefix = std::string(addr) + "/" + std::to_string(route->prefix_len);
    auto key = std::make_pair(route->vrf_id, prefix);

    vppMock().calls.push_back({"ip_route_add_del_get_stats", prefix, route->vrf_id,
            route->nexthop[0].sw_if_index, is_add});

    if (is_add)
    {
        auto& paths = g_fibRoutePaths[key];
        if (!route->is_multipath)
        {
            paths.clear();
        }
        for (uint32_t i = 0; i < route->nexthop_cnt; i++)
        {
            uint32_t sw_if = route->nexthop[i].sw_if_index;
            if (std::find(paths.begin(), paths.end(), sw_if) == paths.end())
            {
                paths.push_back(sw_if);
            }
        }
        return 0;
    }

    if (!route->is_multipath)
    {
        return g_fibRoutePaths.erase(key) == 1 ? 0 : -1;
    }

    auto it = g_fibRoutePaths.find(key);
    if (it == g_fibRoutePaths.end())
    {
        return -1;
    }
    for (uint32_t i = 0; i < route->nexthop_cnt; i++)
    {
        uint32_t sw_if = route->nexthop[i].sw_if_index;
        auto pit = std::find(it->second.begin(), it->second.end(), sw_if);
        if (pit == it->second.end())
        {
            return -1;
        }
        it->second.erase(pit);
    }
    if (it->second.empty())
    {
        g_fibRoutePaths.erase(it);
    }
    return 0;
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
        g_vxlanTunnelDecapAny[*sw_if_index] = tunnel->decap_any;
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

int __wrap_sw_interface_set_mac_by_index(uint32_t sw_if_index, uint8_t *mac_address) { return 0; }
int __wrap_interface_ip_address_add_del(const char *hw_ifname, vpp_ip_route_t *prefix, bool is_add) { return 0; }

int __wrap_ip4_nbr_add_del(const char *hwif_name, uint32_t sw_if_index, struct sockaddr_in *addr,
        bool is_static, bool no_fib_entry, uint8_t *mac, bool is_add)
{
    SWSS_LOG_ENTER();

    vppMock().calls.push_back({"ip4_nbr_add_del", macStr(mac), sw_if_index, 0, is_add});
    return 0;
}

int __wrap_ip6_nbr_add_del(const char *hwif_name, uint32_t sw_if_index, struct sockaddr_in6 *addr,
        bool is_static, bool no_fib_entry, uint8_t *mac, bool is_add) { return 0; }

// every VTEP address in these tests is a local interface address
int __wrap_vpp_sw_interface_find_by_ip(vpp_ip_addr_t *search_ip, uint32_t vrf_id, uint32_t *out_sw_if_index)
{
    SWSS_LOG_ENTER();

    *out_sw_if_index = 1;
    return 0;
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
                vppMock().nodeNexts = nodes;

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
            vppMock().nodeNexts = {
                {"linux-cp-punt", PUNT},
                {"sonic-ext-l2-trap-fixup", TRAP_FIXUP},
                {"sonic-ext-l2-vlan-filter", VLAN_FILTER},
            };
            g_l2Tables.clear();
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

            auto it = g_l2Tables.find(hwif);

            if (it == g_l2Tables.end())
            {
                return MISS;
            }

            bool ip4 = !tagged && frame[12] == 0x08 && frame[13] == 0x00;

            // an untagged IPv4 frame selects the ip4 slot, any tagged frame the other slot
            return classify(ip4 ? it->second[0] : it->second[2], frame);
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

    ASSERT_EQ(3u, vppMock().calls.size());
    EXPECT_EQ("ip_route_add_del", vppMock().calls[0].api);
    EXPECT_EQ("255.255.255.255/32", vppMock().calls[0].name);
    EXPECT_EQ(VRF_TABLE, vppMock().calls[0].id);
    EXPECT_FALSE(vppMock().calls[0].flag);
    EXPECT_EQ("ip_vrf_del", vppMock().calls[1].api);
    EXPECT_EQ("ip_vrf_del", vppMock().calls[2].api);
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

/*
 * A switch with its default virtual router. The fixture reaches the protected
 * and private SwitchVpp members directly, like SwitchVppSubIfAdminState.
 */
class SwitchVppVrf : public ::testing::Test
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
            m_sw = makeSwitch(sc);
            sc->m_useTapDevice = true;

            ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->create_default_virtual_router());

            sai_attribute_t attr;

            attr.id = SAI_SWITCH_ATTR_DEFAULT_VIRTUAL_ROUTER_ID;

            ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->get(SAI_OBJECT_TYPE_SWITCH, sai_serialize_object_id(m_switchId), 1, &attr));

            m_defaultVr = attr.value.oid;

            vppMock().calls.clear();
        }

        virtual std::shared_ptr<SwitchVpp> makeSwitch(
                const std::shared_ptr<SwitchConfig>& sc)
        {
            SWSS_LOG_ENTER();

            return std::make_shared<SwitchVpp>(m_switchId, m_mgr, sc);
        }

        void TearDown() override
        {
            m_sw->m_switchConfig->m_useTapDevice = false;
            m_sw.reset();
        }

        sai_object_id_t createVr()
        {
            SWSS_LOG_ENTER();

            auto vr = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, m_switchId);

            EXPECT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, sai_serialize_object_id(vr), m_switchId, 0, nullptr));

            return vr;
        }

        // A virtual router that exists in SAI but was never given a VPP table.
        sai_object_id_t createVrWithoutTable()
        {
            SWSS_LOG_ENTER();

            auto vr = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, m_switchId);

            EXPECT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create_internal(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, sai_serialize_object_id(vr), m_switchId, 0, nullptr));

            return vr;
        }

        uint32_t tableOf(
                sai_object_id_t vr)
        {
            SWSS_LOG_ENTER();

            auto vrf = m_sw->vpp_get_ip_vrf(vr);

            EXPECT_NE(nullptr, vrf);

            return vrf ? vrf->m_vrf_id : UINT32_MAX;
        }

        sai_status_t createVlanRif(
                sai_object_id_t vr,
                uint16_t vlanId)
        {
            SWSS_LOG_ENTER();

            auto vlan = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_VLAN, m_switchId);

            sai_attribute_t vattr;

            vattr.id = SAI_VLAN_ATTR_VLAN_ID;
            vattr.value.u16 = vlanId;

            EXPECT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create_internal(SAI_OBJECT_TYPE_VLAN, sai_serialize_object_id(vlan), m_switchId, 1, &vattr));

            sai_attribute_t attrs[4];
            sai_mac_t mac = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x01 };

            attrs[0].id = SAI_ROUTER_INTERFACE_ATTR_TYPE;
            attrs[0].value.s32 = SAI_ROUTER_INTERFACE_TYPE_VLAN;
            attrs[1].id = SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID;
            attrs[1].value.oid = vr;
            attrs[2].id = SAI_ROUTER_INTERFACE_ATTR_VLAN_ID;
            attrs[2].value.oid = vlan;
            attrs[3].id = SAI_ROUTER_INTERFACE_ATTR_SRC_MAC_ADDRESS;
            memcpy(attrs[3].value.mac, mac, sizeof(sai_mac_t));

            auto rif = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_ROUTER_INTERFACE, m_switchId);

            return m_sw->create(SAI_OBJECT_TYPE_ROUTER_INTERFACE, sai_serialize_object_id(rif), m_switchId, 4, attrs);
        }

        const sai_object_id_t m_switchId = 0x2100000000;

        std::shared_ptr<RealObjectIdManager> m_mgr;

        std::shared_ptr<SwitchVpp> m_sw;

        sai_object_id_t m_defaultVr = SAI_NULL_OBJECT_ID;
};

TEST_F(SwitchVppVrf, DefaultVirtualRouterUsesTableZero)
{
    ASSERT_EQ(SAI_STATUS_SUCCESS, createVlanRif(m_defaultVr, 100));

    EXPECT_TRUE(vppCallsTo("ip_vrf_add").empty());
    EXPECT_TRUE(vppCallsTo("set_interface_vrf").empty());

    EXPECT_EQ(0u, tableOf(m_defaultVr));

    // registering the default table is what sets its flow hash
    auto hashes = vppCallsTo("vpp_ip_flow_hash_set");

    ASSERT_EQ(2u, hashes.size());
    EXPECT_EQ(0u, hashes[0].id);
    EXPECT_EQ(0u, hashes[1].id);
}

TEST_F(SwitchVppVrf, VirtualRouterGetsItsOwnTable)
{
    const uint32_t base = SwitchVpp::vrf_table_id_base;

    auto vr1 = createVr();
    auto vr2 = createVr();

    EXPECT_EQ(base, tableOf(vr1));
    EXPECT_EQ(base + 1, tableOf(vr2));

    // both address families, named after the table id
    auto adds = vppCallsTo("ip_vrf_add");

    ASSERT_EQ(4u, adds.size());
    EXPECT_EQ(base, adds[0].id);
    EXPECT_FALSE(adds[0].flag);
    EXPECT_EQ("vrf_" + std::to_string(base), adds[0].name);
    EXPECT_EQ(base, adds[1].id);
    EXPECT_TRUE(adds[1].flag);
    EXPECT_EQ("vrf_" + std::to_string(base), m_sw->vpp_get_ip_vrf(vr1)->m_vrf_name);
}

TEST_F(SwitchVppVrf, VlanRouterInterfaceBindsBviToItsVirtualRoutersTable)
{
    auto vr = createVr();
    uint32_t table = tableOf(vr);

    vppMock().calls.clear();

    ASSERT_EQ(SAI_STATUS_SUCCESS, createVlanRif(vr, 100));

    ASSERT_EQ(1u, vppCallsTo("create_bvi_interface").size());

    auto binds = vppCallsTo("set_interface_vrf");

    ASSERT_EQ(2u, binds.size());

    for (auto& bind: binds)
    {
        EXPECT_EQ("bvi100", bind.name);
        EXPECT_EQ(0u, bind.sub_id);
        EXPECT_EQ(table, bind.id);
    }

    EXPECT_FALSE(binds[0].flag);
    EXPECT_TRUE(binds[1].flag);

    // the table was created with the virtual router, not with the interface
    EXPECT_TRUE(vppCallsTo("ip_vrf_add").empty());
}

TEST_F(SwitchVppVrf, SubPortBindsToTheAllocatedTable)
{
    auto vr = createVr();
    uint32_t table = tableOf(vr);

    vppMock().calls.clear();

    // the port and sub-port router interface path
    EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->vpp_router_interface_set_vrf(vr, "GigabitEthernet0/8/0", 100, "Ethernet0.100"));

    auto binds = vppCallsTo("set_interface_vrf");

    ASSERT_EQ(2u, binds.size());
    EXPECT_EQ("GigabitEthernet0/8/0", binds[0].name);
    EXPECT_EQ(100u, binds[0].sub_id);
    EXPECT_EQ(table, binds[0].id);
    EXPECT_EQ(table, binds[1].id);
}

TEST_F(SwitchVppVrf, UnknownVirtualRouterIsNotCachedAsTableZero)
{
    // No VPP table and no kernel VRF to fall back to (the host interface does
    // not exist here): the bind fails, and the virtual router must not be
    // remembered as table 0 for later interfaces.
    auto vr = createVrWithoutTable();

    EXPECT_NE(SAI_STATUS_SUCCESS, m_sw->vpp_router_interface_set_vrf(vr, "bvi200", 0, "Vlan200"));

    EXPECT_TRUE(vppCallsTo("set_interface_vrf").empty());
    EXPECT_EQ(nullptr, m_sw->vpp_get_ip_vrf(vr));
}

TEST_F(SwitchVppVrf, RemovingVirtualRouterReleasesItsTable)
{
    auto vr1 = createVr();
    auto vr2 = createVr();
    uint32_t table1 = tableOf(vr1);

    vppMock().calls.clear();

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->remove(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, sai_serialize_object_id(vr1)));

    auto dels = vppCallsTo("ip_vrf_del");

    ASSERT_EQ(2u, dels.size());
    EXPECT_EQ(table1, dels[0].id);
    EXPECT_FALSE(dels[0].flag);
    EXPECT_EQ(table1, dels[1].id);
    EXPECT_TRUE(dels[1].flag);

    EXPECT_EQ(nullptr, m_sw->vpp_get_ip_vrf(vr1));
    EXPECT_NE(nullptr, m_sw->vpp_get_ip_vrf(vr2));

    // the released id is handed out again
    auto vr3 = createVr();

    EXPECT_EQ(table1, tableOf(vr3));
}

TEST_F(SwitchVppVrf, LinuxNlKeepsKernelTableIds)
{
    // NO_LINUX_NL=n: the linux control plane plugin syncs routes into tables
    // keyed by the kernel table id, so saivpp must not allocate one of its own.
    m_sw->nbr_env_read = true;
    m_sw->nbr_active = false;

    auto vr = createVr();

    EXPECT_TRUE(vppCallsTo("ip_vrf_add").empty());
    EXPECT_EQ(nullptr, m_sw->vpp_get_ip_vrf(vr));
}

/*
 * One P2MP VXLAN tunnel from a local VTEP, with a VR->VNI encap mapper and a
 * VNI->VR decap mapper. Tenant virtual routers A and B have VNIs 1000 and
 * 2000 and no router interfaces yet.
 */
class SwitchVppTunnelNexthop : public SwitchVppVrf
{
    protected:

        void SetUp() override
        {
            SwitchVppVrf::SetUp();

            if (HasFatalFailure())
            {
                return;
            }

            // VXLAN encapsulation reads the switch MAC for the inner source
            sai_attribute_t mac;

            mac.id = SAI_SWITCH_ATTR_SRC_MAC_ADDRESS;
            memcpy(mac.value.mac, SWITCH_MAC, sizeof(sai_mac_t));

            ASSERT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->set_internal(SAI_OBJECT_TYPE_SWITCH, sai_serialize_object_id(m_switchId), &mac));

            m_vrA = createVr();
            m_vrB = createVr();

            m_encapMap = createMap(SAI_TUNNEL_MAP_TYPE_VIRTUAL_ROUTER_ID_TO_VNI);
            m_decapMap = createMap(SAI_TUNNEL_MAP_TYPE_VNI_TO_VIRTUAL_ROUTER_ID);

            addEncapMapEntry(m_vrA, 1000);
            addEncapMapEntry(m_vrB, 2000);

            m_tunnel = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_TUNNEL, m_switchId);

            sai_attribute_t tattrs[4];

            tattrs[0].id = SAI_TUNNEL_ATTR_TYPE;
            tattrs[0].value.s32 = SAI_TUNNEL_TYPE_VXLAN;
            tattrs[1].id = SAI_TUNNEL_ATTR_ENCAP_SRC_IP;
            tattrs[1].value.ipaddr = ipv4("10.0.0.1");
            tattrs[2].id = SAI_TUNNEL_ATTR_ENCAP_MAPPERS;
            tattrs[2].value.objlist.count = 1;
            tattrs[2].value.objlist.list = &m_encapMap;
            tattrs[3].id = SAI_TUNNEL_ATTR_DECAP_MAPPERS;
            tattrs[3].value.objlist.count = 1;
            tattrs[3].value.objlist.list = &m_decapMap;

            ASSERT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create_internal(SAI_OBJECT_TYPE_TUNNEL, sai_serialize_object_id(m_tunnel), m_switchId, 4, tattrs));

            vppMock().calls.clear();
            g_nextSwIfIndex = 100;
            g_vxlanTunnelDecapAny.clear();
            g_fibRoutePaths.clear();
            g_nextRouteStatsIndex = 1;
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

        sai_object_id_t createMap(
                sai_tunnel_map_type_t type)
        {
            SWSS_LOG_ENTER();

            auto map = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_TUNNEL_MAP, m_switchId);

            sai_attribute_t mattr;

            mattr.id = SAI_TUNNEL_MAP_ATTR_TYPE;
            mattr.value.s32 = type;

            EXPECT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create_internal(SAI_OBJECT_TYPE_TUNNEL_MAP, sai_serialize_object_id(map), m_switchId, 1, &mattr));

            return map;
        }

        void addEncapMapEntry(
                sai_object_id_t vr,
                uint32_t vni)
        {
            SWSS_LOG_ENTER();

            auto entry = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_TUNNEL_MAP_ENTRY, m_switchId);

            sai_attribute_t eattrs[4];

            eattrs[0].id = SAI_TUNNEL_MAP_ENTRY_ATTR_TUNNEL_MAP_TYPE;
            eattrs[0].value.s32 = SAI_TUNNEL_MAP_TYPE_VIRTUAL_ROUTER_ID_TO_VNI;
            eattrs[1].id = SAI_TUNNEL_MAP_ENTRY_ATTR_TUNNEL_MAP;
            eattrs[1].value.oid = m_encapMap;
            eattrs[2].id = SAI_TUNNEL_MAP_ENTRY_ATTR_VIRTUAL_ROUTER_ID_KEY;
            eattrs[2].value.oid = vr;
            eattrs[3].id = SAI_TUNNEL_MAP_ENTRY_ATTR_VNI_ID_VALUE;
            eattrs[3].value.u32 = vni;

            ASSERT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create_internal(SAI_OBJECT_TYPE_TUNNEL_MAP_ENTRY, sai_serialize_object_id(entry), m_switchId, 4, eattrs));
        }

        sai_object_id_t createEncapMapEntry(
                sai_object_id_t vr,
                uint32_t vni)
        {
            SWSS_LOG_ENTER();

            auto entry = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_TUNNEL_MAP_ENTRY, m_switchId);

            sai_attribute_t eattrs[4];

            eattrs[0].id = SAI_TUNNEL_MAP_ENTRY_ATTR_TUNNEL_MAP_TYPE;
            eattrs[0].value.s32 = SAI_TUNNEL_MAP_TYPE_VIRTUAL_ROUTER_ID_TO_VNI;
            eattrs[1].id = SAI_TUNNEL_MAP_ENTRY_ATTR_TUNNEL_MAP;
            eattrs[1].value.oid = m_encapMap;
            eattrs[2].id = SAI_TUNNEL_MAP_ENTRY_ATTR_VIRTUAL_ROUTER_ID_KEY;
            eattrs[2].value.oid = vr;
            eattrs[3].id = SAI_TUNNEL_MAP_ENTRY_ATTR_VNI_ID_VALUE;
            eattrs[3].value.u32 = vni;

            EXPECT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create(SAI_OBJECT_TYPE_TUNNEL_MAP_ENTRY, sai_serialize_object_id(entry), m_switchId, 4, eattrs));

            return entry;
        }

        // what orchagent creates for the VNI of a VRF, after the tunnel
        sai_status_t createDecapMapEntry(
                sai_object_id_t vr,
                uint32_t vni,
                sai_object_id_t *entryOut = nullptr)
        {
            SWSS_LOG_ENTER();

            auto entry = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_TUNNEL_MAP_ENTRY, m_switchId);

            if (entryOut)
            {
                *entryOut = entry;
            }

            sai_attribute_t eattrs[4];

            eattrs[0].id = SAI_TUNNEL_MAP_ENTRY_ATTR_TUNNEL_MAP_TYPE;
            eattrs[0].value.s32 = SAI_TUNNEL_MAP_TYPE_VNI_TO_VIRTUAL_ROUTER_ID;
            eattrs[1].id = SAI_TUNNEL_MAP_ENTRY_ATTR_TUNNEL_MAP;
            eattrs[1].value.oid = m_decapMap;
            eattrs[2].id = SAI_TUNNEL_MAP_ENTRY_ATTR_VNI_ID_KEY;
            eattrs[2].value.u32 = vni;
            eattrs[3].id = SAI_TUNNEL_MAP_ENTRY_ATTR_VIRTUAL_ROUTER_ID_VALUE;
            eattrs[3].value.oid = vr;

            return m_sw->create(SAI_OBJECT_TYPE_TUNNEL_MAP_ENTRY, sai_serialize_object_id(entry), m_switchId, 4, eattrs);
        }

        // VNI 0 creates a next hop without SAI_NEXT_HOP_ATTR_TUNNEL_VNI
        sai_status_t createNexthop(
                uint32_t vni,
                sai_object_id_t& nh,
                const uint8_t *remoteRouterMac = nullptr)
        {
            SWSS_LOG_ENTER();

            nh = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_NEXT_HOP, m_switchId);

            sai_attribute_t attrs[5];
            uint32_t count = 3;

            attrs[0].id = SAI_NEXT_HOP_ATTR_TYPE;
            attrs[0].value.s32 = SAI_NEXT_HOP_TYPE_TUNNEL_ENCAP;
            attrs[1].id = SAI_NEXT_HOP_ATTR_IP;
            attrs[1].value.ipaddr = ipv4("10.0.0.2");
            attrs[2].id = SAI_NEXT_HOP_ATTR_TUNNEL_ID;
            attrs[2].value.oid = m_tunnel;

            if (vni)
            {
                attrs[count].id = SAI_NEXT_HOP_ATTR_TUNNEL_VNI;
                attrs[count].value.u32 = vni;
                count++;
            }

            if (remoteRouterMac)
            {
                attrs[count].id = SAI_NEXT_HOP_ATTR_TUNNEL_MAC;
                memcpy(attrs[count].value.mac, remoteRouterMac, sizeof(sai_mac_t));
                count++;
            }

            return m_sw->create(SAI_OBJECT_TYPE_NEXT_HOP, sai_serialize_object_id(nh), m_switchId, count, attrs);
        }

        std::string createIpv4Route(
                sai_object_id_t vr,
                const char *ip,
                const char *mask,
                sai_object_id_t nextHopOrNhg)
        {
            SWSS_LOG_ENTER();

            sai_route_entry_t entry = {};

            entry.switch_id = m_switchId;
            entry.vr_id = vr;
            entry.destination.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
            EXPECT_EQ(1, inet_pton(AF_INET, ip, &entry.destination.addr.ip4));
            EXPECT_EQ(1, inet_pton(AF_INET, mask, &entry.destination.mask.ip4));

            std::string serialized = sai_serialize_route_entry(entry);

            sai_attribute_t attrs[2];

            attrs[0].id = SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION;
            attrs[0].value.s32 = SAI_PACKET_ACTION_FORWARD;
            attrs[1].id = SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID;
            attrs[1].value.oid = nextHopOrNhg;

            EXPECT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create(SAI_OBJECT_TYPE_ROUTE_ENTRY, serialized, m_switchId, 2, attrs));

            return serialized;
        }

        // the set_interface_vrf calls for one interface
        std::vector<VppCall> bindsOf(
                const std::string& hwif)
        {
            SWSS_LOG_ENTER();

            return vppCallsTo("set_interface_vrf", hwif);
        }

        static constexpr uint8_t SWITCH_MAC[6] = { 0x22, 0xfd, 0xe1, 0x99, 0x49, 0x9e };

        sai_object_id_t m_vrA = SAI_NULL_OBJECT_ID;
        sai_object_id_t m_vrB = SAI_NULL_OBJECT_ID;
        sai_object_id_t m_encapMap = SAI_NULL_OBJECT_ID;
        sai_object_id_t m_decapMap = SAI_NULL_OBJECT_ID;
        sai_object_id_t m_tunnel = SAI_NULL_OBJECT_ID;
};

constexpr uint8_t SwitchVppTunnelNexthop::SWITCH_MAC[6];

/*
 * The decap term of an L3VNI needs the VPP table of the VNI's virtual router.
 * An EVPN tenant VRF usually has no router interface other than its L3VNI
 * SVI, and orchagent may map the VNI before creating that SVI. The table is
 * there anyway because the virtual router got it when it was created.
 */
TEST_F(SwitchVppTunnelNexthop, DecapTermOfAVirtualRouterWithNoRouterInterfaceRoutesInItsTable)
{
    ASSERT_EQ(SAI_STATUS_SUCCESS, createDecapMapEntry(m_vrB, 2000));

    auto bvis = vppCallsTo("create_bvi_interface");

    ASSERT_EQ(1u, bvis.size());

    auto bvi = "bvi" + std::to_string(bvis[0].id);
    auto binds = bindsOf(bvi);
    uint32_t table = tableOf(m_vrB);

    // both address families: the tunnel carries IPv4 and IPv6
    ASSERT_EQ(2u, binds.size());
    EXPECT_EQ(table, binds[0].id);
    EXPECT_FALSE(binds[0].flag);
    EXPECT_EQ(table, binds[1].id);
    EXPECT_TRUE(binds[1].flag);

    auto tunnels = vppCallsTo("vpp_vxlan_tunnel_add_del");

    ASSERT_EQ(1u, tunnels.size());
    EXPECT_EQ(2000u, tunnels[0].id);
    EXPECT_TRUE(g_vxlanTunnelDecapAny.at(tunnels[0].sub_id));
}

TEST_F(SwitchVppTunnelNexthop, NexthopDecapBvisRouteInBothAddressFamilies)
{
    sai_object_id_t nh;

    // without a VNI of its own the next hop gets a tunnel per mapped VNI
    ASSERT_EQ(SAI_STATUS_SUCCESS, createNexthop(0, nh));

    auto bvis = vppCallsTo("create_bvi_interface");

    ASSERT_EQ(2u, bvis.size());

    for (auto& created: bvis)
    {
        auto binds = bindsOf("bvi" + std::to_string(created.id));

        ASSERT_EQ(2u, binds.size());
        EXPECT_EQ(binds[0].id, binds[1].id);
        EXPECT_FALSE(binds[0].flag);
        EXPECT_TRUE(binds[1].flag);
    }
}

namespace
{
    // what an EVPN type-5 next hop carries: the remote VTEP's router MAC
    const uint8_t REMOTE_ROUTER_MAC[6] = { 0x22, 0x36, 0x29, 0xc2, 0x14, 0xf1 };
}

/*
 * An EVPN next hop names its VNI. VPP matches the exact (remote, local, VNI)
 * key of its tunnel before the source-independent decap term of that VNI, so
 * the next hop's own tunnel has to decapsulate, into the VRF of the VNI.
 */
TEST_F(SwitchVppTunnelNexthop, NexthopWithAVniDecapsulatesInTheVrfOfItsVni)
{
    sai_object_id_t nh;

    ASSERT_EQ(SAI_STATUS_SUCCESS, createNexthop(2000, nh, REMOTE_ROUTER_MAC));

    auto tunnels = vppCallsTo("vpp_vxlan_tunnel_add_del");

    ASSERT_EQ(1u, tunnels.size());
    EXPECT_EQ(2000u, tunnels[0].id);
    EXPECT_TRUE(g_vxlanTunnelDecapAny.at(tunnels[0].sub_id));

    auto bvis = vppCallsTo("create_bvi_interface");

    ASSERT_EQ(1u, bvis.size());

    auto binds = bindsOf("bvi" + std::to_string(bvis[0].id));
    uint32_t table = tableOf(m_vrB);

    ASSERT_EQ(2u, binds.size());
    EXPECT_EQ(table, binds[0].id);
    EXPECT_EQ(table, binds[1].id);

    // a route in any VRF still uses the tunnel, a leaked route included
    uint32_t inB = 0;
    uint32_t inA = 0;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->m_tunnel_mgr.get_tunnel_if(nh, m_vrB, inB));
    EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->m_tunnel_mgr.get_tunnel_if(nh, m_vrA, inA));
    EXPECT_EQ(inB, inA);
}

TEST_F(SwitchVppTunnelNexthop, NexthopWithAnUnmappedVniHasNoDecap)
{
    sai_object_id_t nh;

    ASSERT_EQ(SAI_STATUS_SUCCESS, createNexthop(4000, nh, REMOTE_ROUTER_MAC));

    auto tunnels = vppCallsTo("vpp_vxlan_tunnel_add_del");

    ASSERT_EQ(1u, tunnels.size());
    EXPECT_FALSE(g_vxlanTunnelDecapAny.at(tunnels[0].sub_id));
    EXPECT_TRUE(vppCallsTo("create_bvi_interface").empty());

    uint32_t sw_if_index = 0;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->m_tunnel_mgr.get_tunnel_if(nh, m_vrA, sw_if_index));
}

TEST_F(SwitchVppTunnelNexthop, RemovingANexthopWithAVniTearsDownItsDecap)
{
    sai_object_id_t nh;

    ASSERT_EQ(SAI_STATUS_SUCCESS, createNexthop(2000, nh, REMOTE_ROUTER_MAC));

    auto bvis = vppCallsTo("create_bvi_interface");

    ASSERT_EQ(1u, bvis.size());

    vppMock().calls.clear();

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->remove(SAI_OBJECT_TYPE_NEXT_HOP, sai_serialize_object_id(nh)));

    auto tunnels = vppCallsTo("vpp_vxlan_tunnel_add_del");

    ASSERT_EQ(1u, tunnels.size());
    EXPECT_EQ(2000u, tunnels[0].id);
    EXPECT_FALSE(tunnels[0].flag);

    EXPECT_EQ(1u, vppCallsTo("delete_bvi_interface", "bvi" + std::to_string(bvis[0].id)).size());
}

TEST_F(SwitchVppTunnelNexthop, TunnelMapEntryArrivingAfterNexthopInstallsDecap)
{
    sai_object_id_t nh;

    ASSERT_EQ(SAI_STATUS_SUCCESS, createNexthop(4000, nh, REMOTE_ROUTER_MAC));
    EXPECT_TRUE(vppCallsTo("create_bvi_interface").empty());

    uint32_t initial_sw_if = 0;

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->m_tunnel_mgr.get_tunnel_if(nh, m_vrA, initial_sw_if));
    EXPECT_FALSE(g_vxlanTunnelDecapAny.at(initial_sw_if));

    vppMock().calls.clear();

    sai_object_id_t mapEntry = createEncapMapEntry(m_vrB, 4000);

    auto tunnels = vppCallsTo("vpp_vxlan_tunnel_add_del");

    ASSERT_EQ(2u, tunnels.size());
    EXPECT_EQ(4000u, tunnels[0].id);
    EXPECT_FALSE(tunnels[0].flag);
    EXPECT_EQ(4000u, tunnels[1].id);
    EXPECT_TRUE(tunnels[1].flag);

    uint32_t recreated_sw_if = tunnels[1].sub_id;

    EXPECT_NE(initial_sw_if, recreated_sw_if);
    EXPECT_TRUE(g_vxlanTunnelDecapAny.at(recreated_sw_if));

    auto bvis = vppCallsTo("create_bvi_interface");

    ASSERT_EQ(1u, bvis.size());

    std::string bvi = "bvi" + std::to_string(bvis[0].id);
    auto binds = bindsOf(bvi);
    uint32_t table = tableOf(m_vrB);

    ASSERT_EQ(2u, binds.size());
    EXPECT_EQ(table, binds[0].id);
    EXPECT_FALSE(binds[0].flag);
    EXPECT_EQ(table, binds[1].id);
    EXPECT_TRUE(binds[1].flag);

    auto bridged = vppCallsTo("set_sw_interface_l2_bridge_shg_by_index", std::to_string(recreated_sw_if));

    ASSERT_EQ(1u, bridged.size());
    EXPECT_EQ(bvis[0].id, bridged[0].id);
    EXPECT_TRUE(bridged[0].flag);

    uint32_t afterInB = 0;
    uint32_t afterInA = 0;

    EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->m_tunnel_mgr.get_tunnel_if(nh, m_vrB, afterInB));
    EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->m_tunnel_mgr.get_tunnel_if(nh, m_vrA, afterInA));
    EXPECT_EQ(recreated_sw_if, afterInB);
    EXPECT_EQ(recreated_sw_if, afterInA);

    vppMock().calls.clear();

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->remove(SAI_OBJECT_TYPE_TUNNEL_MAP_ENTRY, sai_serialize_object_id(mapEntry)));
    EXPECT_EQ(1u, vppCallsTo("delete_bvi_interface", bvi).size());

    tunnels = vppCallsTo("vpp_vxlan_tunnel_add_del");

    ASSERT_EQ(2u, tunnels.size());
    EXPECT_EQ(4000u, tunnels[0].id);
    EXPECT_FALSE(tunnels[0].flag);
    EXPECT_EQ(4000u, tunnels[1].id);
    EXPECT_TRUE(tunnels[1].flag);

    uint32_t reverted_sw_if = tunnels[1].sub_id;

    EXPECT_NE(recreated_sw_if, reverted_sw_if);
    EXPECT_FALSE(g_vxlanTunnelDecapAny.at(reverted_sw_if));

    vppMock().calls.clear();

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->remove(SAI_OBJECT_TYPE_NEXT_HOP, sai_serialize_object_id(nh)));

    tunnels = vppCallsTo("vpp_vxlan_tunnel_add_del");

    ASSERT_EQ(1u, tunnels.size());
    EXPECT_EQ(4000u, tunnels[0].id);
    EXPECT_FALSE(tunnels[0].flag);
    EXPECT_TRUE(vppCallsTo("delete_bvi_interface").empty());
}

TEST_F(SwitchVppTunnelNexthop, DecapMapEntryArrivingAfterNexthopInstallsDecap)
{
    sai_object_id_t nh;

    ASSERT_EQ(SAI_STATUS_SUCCESS, createNexthop(4000, nh, REMOTE_ROUTER_MAC));
    EXPECT_TRUE(vppCallsTo("create_bvi_interface").empty());

    uint32_t initial_sw_if = 0;

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->m_tunnel_mgr.get_tunnel_if(nh, m_vrA, initial_sw_if));
    EXPECT_FALSE(g_vxlanTunnelDecapAny.at(initial_sw_if));

    vppMock().calls.clear();

    sai_object_id_t decapEntry = SAI_NULL_OBJECT_ID;

    ASSERT_EQ(SAI_STATUS_SUCCESS, createDecapMapEntry(m_vrB, 4000, &decapEntry));

    uint32_t recreated_sw_if = 0;

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->m_tunnel_mgr.get_tunnel_if(nh, m_vrA, recreated_sw_if));
    EXPECT_NE(initial_sw_if, recreated_sw_if);
    EXPECT_TRUE(g_vxlanTunnelDecapAny.at(recreated_sw_if));

    auto bvis = vppCallsTo("create_bvi_interface");

    ASSERT_EQ(2u, bvis.size());

    std::string nhBvi = "bvi" + std::to_string(bvis[0].id);
    std::string termBvi = "bvi" + std::to_string(bvis[1].id);
    uint32_t table = tableOf(m_vrB);

    for (const auto& created : bvis)
    {
        auto binds = bindsOf("bvi" + std::to_string(created.id));

        ASSERT_EQ(2u, binds.size());
        EXPECT_EQ(table, binds[0].id);
        EXPECT_FALSE(binds[0].flag);
        EXPECT_EQ(table, binds[1].id);
        EXPECT_TRUE(binds[1].flag);
    }

    auto bridged = vppCallsTo("set_sw_interface_l2_bridge_shg_by_index", std::to_string(recreated_sw_if));

    ASSERT_EQ(1u, bridged.size());
    EXPECT_EQ(bvis[0].id, bridged[0].id);
    EXPECT_TRUE(bridged[0].flag);

    vppMock().calls.clear();

    sai_object_id_t encapEntry = createEncapMapEntry(m_vrB, 4000);

    EXPECT_TRUE(vppCallsTo("create_bvi_interface").empty());
    EXPECT_TRUE(vppCallsTo("vpp_vxlan_tunnel_add_del").empty());

    vppMock().calls.clear();

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->remove(SAI_OBJECT_TYPE_TUNNEL_MAP_ENTRY, sai_serialize_object_id(decapEntry)));
    EXPECT_TRUE(vppCallsTo("delete_bvi_interface", nhBvi).empty());
    EXPECT_EQ(1u, vppCallsTo("delete_bvi_interface", termBvi).size());
    EXPECT_TRUE(g_vxlanTunnelDecapAny.at(recreated_sw_if));

    vppMock().calls.clear();

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->remove(SAI_OBJECT_TYPE_TUNNEL_MAP_ENTRY, sai_serialize_object_id(encapEntry)));
    EXPECT_EQ(1u, vppCallsTo("delete_bvi_interface", nhBvi).size());

    uint32_t reverted_sw_if = 0;

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->m_tunnel_mgr.get_tunnel_if(nh, m_vrA, reverted_sw_if));
    EXPECT_NE(recreated_sw_if, reverted_sw_if);
    EXPECT_FALSE(g_vxlanTunnelDecapAny.at(reverted_sw_if));
}

TEST_F(SwitchVppTunnelNexthop, LateTunnelMapEntryRebindsDirectAndEcmpRoutesToRecreatedTunnel)
{
    sai_object_id_t nh;

    ASSERT_EQ(SAI_STATUS_SUCCESS, createNexthop(4000, nh, REMOTE_ROUTER_MAC));

    uint32_t initial_sw_if = 0;

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->m_tunnel_mgr.get_tunnel_if(nh, m_vrA, initial_sw_if));
    ASSERT_FALSE(g_vxlanTunnelDecapAny.at(initial_sw_if));

    std::string directRoute = createIpv4Route(m_vrA, "192.0.2.0", "255.255.255.0", nh);

    sai_object_id_t nhg = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_NEXT_HOP_GROUP, m_switchId);
    sai_attribute_t nhgAttr;

    nhgAttr.id = SAI_NEXT_HOP_GROUP_ATTR_TYPE;
    nhgAttr.value.s32 = SAI_NEXT_HOP_GROUP_TYPE_DYNAMIC_UNORDERED_ECMP;
    ASSERT_EQ(SAI_STATUS_SUCCESS,
            m_sw->create(SAI_OBJECT_TYPE_NEXT_HOP_GROUP, sai_serialize_object_id(nhg), m_switchId, 1, &nhgAttr));

    sai_object_id_t nhgMbr = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_NEXT_HOP_GROUP_MEMBER, m_switchId);
    sai_attribute_t mbrAttrs[2];

    mbrAttrs[0].id = SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_GROUP_ID;
    mbrAttrs[0].value.oid = nhg;
    mbrAttrs[1].id = SAI_NEXT_HOP_GROUP_MEMBER_ATTR_NEXT_HOP_ID;
    mbrAttrs[1].value.oid = nh;
    ASSERT_EQ(SAI_STATUS_SUCCESS,
            m_sw->create(SAI_OBJECT_TYPE_NEXT_HOP_GROUP_MEMBER, sai_serialize_object_id(nhgMbr), m_switchId, 2, mbrAttrs));

    std::string ecmpRoute = createIpv4Route(m_vrA, "198.51.100.0", "255.255.255.0", nhg);

    uint32_t tableA = tableOf(m_vrA);
    const auto directKey = std::make_pair(tableA, std::string("192.0.2.0/24"));
    const auto ecmpKey = std::make_pair(tableA, std::string("198.51.100.0/24"));

    EXPECT_EQ((std::vector<uint32_t>{initial_sw_if}), g_fibRoutePaths.at(directKey));
    EXPECT_EQ((std::vector<uint32_t>{initial_sw_if}), g_fibRoutePaths.at(ecmpKey));

    sai_object_id_t mapEntry = createEncapMapEntry(m_vrB, 4000);

    uint32_t recreated_sw_if = 0;

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->m_tunnel_mgr.get_tunnel_if(nh, m_vrA, recreated_sw_if));
    EXPECT_NE(initial_sw_if, recreated_sw_if);
    EXPECT_TRUE(g_vxlanTunnelDecapAny.at(recreated_sw_if));
    EXPECT_EQ((std::vector<uint32_t>{recreated_sw_if}), g_fibRoutePaths.at(directKey));
    EXPECT_EQ((std::vector<uint32_t>{recreated_sw_if}), g_fibRoutePaths.at(ecmpKey));

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->remove(SAI_OBJECT_TYPE_TUNNEL_MAP_ENTRY, sai_serialize_object_id(mapEntry)));

    uint32_t reverted_sw_if = 0;

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->m_tunnel_mgr.get_tunnel_if(nh, m_vrA, reverted_sw_if));
    EXPECT_NE(recreated_sw_if, reverted_sw_if);
    EXPECT_FALSE(g_vxlanTunnelDecapAny.at(reverted_sw_if));
    EXPECT_EQ((std::vector<uint32_t>{reverted_sw_if}), g_fibRoutePaths.at(directKey));
    EXPECT_EQ((std::vector<uint32_t>{reverted_sw_if}), g_fibRoutePaths.at(ecmpKey));

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->remove(SAI_OBJECT_TYPE_ROUTE_ENTRY, directRoute));
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->remove(SAI_OBJECT_TYPE_ROUTE_ENTRY, ecmpRoute));
    EXPECT_EQ(0u, g_fibRoutePaths.count(directKey));
    EXPECT_EQ(0u, g_fibRoutePaths.count(ecmpKey));
}

TEST_F(SwitchVppTunnelNexthop, P2pTunnelKeepsSourceCheckWhenMapEntryArrivesLate)
{
    sai_object_id_t p2pTunnel = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_TUNNEL, m_switchId);
    sai_attribute_t tattrs[6];

    tattrs[0].id = SAI_TUNNEL_ATTR_TYPE;
    tattrs[0].value.s32 = SAI_TUNNEL_TYPE_VXLAN;
    tattrs[1].id = SAI_TUNNEL_ATTR_PEER_MODE;
    tattrs[1].value.s32 = SAI_TUNNEL_PEER_MODE_P2P;
    tattrs[2].id = SAI_TUNNEL_ATTR_ENCAP_SRC_IP;
    tattrs[2].value.ipaddr = ipv4("10.0.0.1");
    tattrs[3].id = SAI_TUNNEL_ATTR_ENCAP_DST_IP;
    tattrs[3].value.ipaddr = ipv4("10.0.0.3");
    tattrs[4].id = SAI_TUNNEL_ATTR_ENCAP_MAPPERS;
    tattrs[4].value.objlist.count = 1;
    tattrs[4].value.objlist.list = &m_encapMap;
    tattrs[5].id = SAI_TUNNEL_ATTR_DECAP_MAPPERS;
    tattrs[5].value.objlist.count = 1;
    tattrs[5].value.objlist.list = &m_decapMap;

    ASSERT_EQ(SAI_STATUS_SUCCESS,
            m_sw->create_internal(SAI_OBJECT_TYPE_TUNNEL, sai_serialize_object_id(p2pTunnel), m_switchId, 6, tattrs));

    sai_object_id_t nh = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_NEXT_HOP, m_switchId);
    sai_attribute_t nhAttrs[5];

    nhAttrs[0].id = SAI_NEXT_HOP_ATTR_TYPE;
    nhAttrs[0].value.s32 = SAI_NEXT_HOP_TYPE_TUNNEL_ENCAP;
    nhAttrs[1].id = SAI_NEXT_HOP_ATTR_IP;
    nhAttrs[1].value.ipaddr = ipv4("10.0.0.3");
    nhAttrs[2].id = SAI_NEXT_HOP_ATTR_TUNNEL_ID;
    nhAttrs[2].value.oid = p2pTunnel;
    nhAttrs[3].id = SAI_NEXT_HOP_ATTR_TUNNEL_VNI;
    nhAttrs[3].value.u32 = 4000;
    nhAttrs[4].id = SAI_NEXT_HOP_ATTR_TUNNEL_MAC;
    memcpy(nhAttrs[4].value.mac, REMOTE_ROUTER_MAC, sizeof(sai_mac_t));

    ASSERT_EQ(SAI_STATUS_SUCCESS,
            m_sw->create(SAI_OBJECT_TYPE_NEXT_HOP, sai_serialize_object_id(nh), m_switchId, 5, nhAttrs));

    uint32_t sw_if_index = 0;

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->m_tunnel_mgr.get_tunnel_if(nh, m_vrA, sw_if_index));
    EXPECT_FALSE(g_vxlanTunnelDecapAny.at(sw_if_index));

    vppMock().calls.clear();

    createEncapMapEntry(m_vrB, 4000);

    EXPECT_EQ(1u, vppCallsTo("create_bvi_interface").size());
    EXPECT_TRUE(vppCallsTo("vpp_vxlan_tunnel_add_del").empty());
    EXPECT_FALSE(g_vxlanTunnelDecapAny.at(sw_if_index));
}

/*
 * The kernel side of the decap host path, replaced: which VRF device each
 * kernel netdev is enslaved to, IPv6 disables, and the redirects asked for.
 */
class SwitchVppDecapHostPath : public SwitchVppTunnelNexthop
{
    protected:

        std::shared_ptr<SwitchVpp> makeSwitch(
                const std::shared_ptr<SwitchConfig>& sc) override
        {
            SWSS_LOG_ENTER();

            return std::make_shared<TestableSwitchVpp>(m_switchId, m_mgr, sc);
        }

        void SetUp() override
        {
            SwitchVppTunnelNexthop::SetUp();

            if (HasFatalFailure())
            {
                return;
            }

            m_sw->m_tunnel_mgr.m_netdev_master = [this](const std::string& netdev)
            {
                SWSS_LOG_ENTER();

                auto it = m_kernelMasters.find(netdev);

                return it == m_kernelMasters.end() ? std::string() : it->second;
            };

            m_sw->m_tunnel_mgr.m_disable_ipv6 = [this](const std::string& tap)
            {
                SWSS_LOG_ENTER();

                m_ipv6Disabled.push_back(tap);

                return SAI_STATUS_SUCCESS;
            };

            m_sw->m_tunnel_mgr.m_add_tc_redirect = [this](const std::string& tap, const std::string& dev)
            {
                SWSS_LOG_ENTER();

                m_redirects.push_back(tap + " " + dev);

                return m_redirectStatus;
            };

            m_sw->m_tunnel_mgr.m_del_tc_redirect = [this](const std::string& tap)
            {
                SWSS_LOG_ENTER();

                m_deletedRedirects.push_back(tap);
            };
        }

        sai_object_id_t registerPhysicalPort(
                const std::string& hwif,
                const std::string& osif)
        {
            SWSS_LOG_ENTER();

            auto port = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_PORT, m_switchId);
            EXPECT_NE(nullptr, m_sw->m_ifaceRegistry.addPhysicalPort(hwif, osif, port));
            EXPECT_TRUE(m_sw->m_ifaceRegistry.setTapName(hwif, osif));
            return port;
        }

        sai_object_id_t registerLag(
                uint32_t bondId)
        {
            SWSS_LOG_ENTER();

            auto lag = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_LAG, m_switchId);
            EXPECT_NE(nullptr, m_sw->m_ifaceRegistry.addLag(bondId, bondId, lag));
            EXPECT_TRUE(m_sw->m_ifaceRegistry.setTapName(
                        "BondEthernet" + std::to_string(bondId),
                        "be" + std::to_string(bondId)));
            return lag;
        }

        sai_status_t createPortRif(
                sai_object_id_t vr,
                sai_object_id_t portOrLag)
        {
            SWSS_LOG_ENTER();

            auto rif = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_ROUTER_INTERFACE, m_switchId);
            sai_attribute_t attrs[4] = {};

            attrs[0].id = SAI_ROUTER_INTERFACE_ATTR_TYPE;
            attrs[0].value.s32 = SAI_ROUTER_INTERFACE_TYPE_PORT;
            attrs[1].id = SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID;
            attrs[1].value.oid = vr;
            attrs[2].id = SAI_ROUTER_INTERFACE_ATTR_PORT_ID;
            attrs[2].value.oid = portOrLag;
            attrs[3].id = SAI_ROUTER_INTERFACE_ATTR_SRC_MAC_ADDRESS;
            memcpy(attrs[3].value.mac, SWITCH_MAC, sizeof(sai_mac_t));

            return m_sw->create(SAI_OBJECT_TYPE_ROUTER_INTERFACE, sai_serialize_object_id(rif), m_switchId, 4, attrs);
        }

        sai_status_t createSubPortRif(
                sai_object_id_t vr,
                sai_object_id_t portOrLag,
                uint16_t vlanId)
        {
            SWSS_LOG_ENTER();

            auto rif = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_ROUTER_INTERFACE, m_switchId);
            sai_attribute_t attrs[5] = {};

            attrs[0].id = SAI_ROUTER_INTERFACE_ATTR_TYPE;
            attrs[0].value.s32 = SAI_ROUTER_INTERFACE_TYPE_SUB_PORT;
            attrs[1].id = SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID;
            attrs[1].value.oid = vr;
            attrs[2].id = SAI_ROUTER_INTERFACE_ATTR_PORT_ID;
            attrs[2].value.oid = portOrLag;
            attrs[3].id = SAI_ROUTER_INTERFACE_ATTR_OUTER_VLAN_ID;
            attrs[3].value.u16 = vlanId;
            attrs[4].id = SAI_ROUTER_INTERFACE_ATTR_SRC_MAC_ADDRESS;
            memcpy(attrs[4].value.mac, SWITCH_MAC, sizeof(sai_mac_t));

            return m_sw->create(SAI_OBJECT_TYPE_ROUTER_INTERFACE, sai_serialize_object_id(rif), m_switchId, 5, attrs);
        }

        // name of the one decap BVI created since the last clear
        std::string decapBvi()
        {
            SWSS_LOG_ENTER();

            auto bvis = vppCallsTo("create_bvi_interface");

            EXPECT_EQ(1u, bvis.size());

            return bvis.empty() ? std::string() : "bvi" + std::to_string(bvis[0].id);
        }

        std::map<std::string, std::string> m_kernelMasters;

        std::vector<std::string> m_ipv6Disabled;

        std::vector<std::string> m_redirects;

        std::vector<std::string> m_deletedRedirects;

        sai_status_t m_redirectStatus = SAI_STATUS_SUCCESS;
};

TEST_F(SwitchVppDecapHostPath, DecapBviGetsAHostPathIntoItsKernelVrf)
{
    ASSERT_EQ(SAI_STATUS_SUCCESS, createVlanRif(m_vrB, 3000));

    m_kernelMasters["Vlan3000"] = "Vrft";

    vppMock().calls.clear();

    sai_object_id_t nh;

    ASSERT_EQ(SAI_STATUS_SUCCESS, createNexthop(2000, nh, REMOTE_ROUTER_MAC));

    auto bvi = decapBvi();

    int pair = vppCallIndex("configure_lcp_interface", bvi + " tap_" + bvi);
    int bridge = vppCallIndex("set_sw_interface_l2_bridge", bvi);

    ASSERT_NE(-1, pair);
    ASSERT_NE(-1, bridge);
    EXPECT_TRUE(vppMock().calls[pair].flag);

    // paired before it becomes its bridge domain's BVI, or sonic_ext steers
    // the punts to the underlay port's tap
    EXPECT_LT(pair, bridge);

    ASSERT_EQ(1u, m_ipv6Disabled.size());
    EXPECT_EQ("tap_" + bvi, m_ipv6Disabled[0]);

    ASSERT_EQ(1u, m_redirects.size());
    EXPECT_EQ("tap_" + bvi + " Vrft", m_redirects[0]);
}

TEST_F(SwitchVppDecapHostPath, LateTunnelMapEntryGivesDecapBviAHostPath)
{
    ASSERT_EQ(SAI_STATUS_SUCCESS, createVlanRif(m_vrB, 3000));

    m_kernelMasters["Vlan3000"] = "Vrft";

    sai_object_id_t nh;

    ASSERT_EQ(SAI_STATUS_SUCCESS, createNexthop(4000, nh, REMOTE_ROUTER_MAC));

    vppMock().calls.clear();
    m_ipv6Disabled.clear();
    m_redirects.clear();

    sai_object_id_t mapEntry = createEncapMapEntry(m_vrB, 4000);

    auto bvi = decapBvi();

    int pair = vppCallIndex("configure_lcp_interface", bvi + " tap_" + bvi);
    int bridge = vppCallIndex("set_sw_interface_l2_bridge", bvi);

    ASSERT_NE(-1, pair);
    ASSERT_NE(-1, bridge);
    EXPECT_TRUE(vppMock().calls[pair].flag);
    EXPECT_LT(pair, bridge);

    ASSERT_EQ(1u, m_ipv6Disabled.size());
    EXPECT_EQ("tap_" + bvi, m_ipv6Disabled[0]);

    ASSERT_EQ(1u, m_redirects.size());
    EXPECT_EQ("tap_" + bvi + " Vrft", m_redirects[0]);

    vppMock().calls.clear();

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->remove(SAI_OBJECT_TYPE_TUNNEL_MAP_ENTRY, sai_serialize_object_id(mapEntry)));

    int unpair = vppCallIndex("configure_lcp_interface", bvi + " tap_" + bvi);
    int del = vppCallIndex("delete_bvi_interface", bvi);

    ASSERT_NE(-1, unpair);
    ASSERT_NE(-1, del);
    EXPECT_FALSE(vppMock().calls[unpair].flag);
    EXPECT_LT(unpair, del);
}

TEST_F(SwitchVppDecapHostPath, NoKernelVrfMeansNoHostPath)
{
    // the VR has a VLAN router interface, but its SVI is in no kernel VRF
    ASSERT_EQ(SAI_STATUS_SUCCESS, createVlanRif(m_vrB, 3000));

    vppMock().calls.clear();

    sai_object_id_t nh;

    ASSERT_EQ(SAI_STATUS_SUCCESS, createNexthop(2000, nh, REMOTE_ROUTER_MAC));

    EXPECT_TRUE(vppCallsTo("configure_lcp_interface").empty());
    EXPECT_TRUE(m_redirects.empty());
}

TEST_F(SwitchVppDecapHostPath, DoesNotBorrowTheKernelVrfOfAnotherVirtualRouter)
{
    ASSERT_EQ(SAI_STATUS_SUCCESS, createVlanRif(m_vrA, 100));

    m_kernelMasters["Vlan100"] = "VrfA";

    vppMock().calls.clear();

    sai_object_id_t nh;

    // VNI 2000 belongs to VR B
    ASSERT_EQ(SAI_STATUS_SUCCESS, createNexthop(2000, nh, REMOTE_ROUTER_MAC));

    EXPECT_TRUE(vppCallsTo("configure_lcp_interface").empty());
    EXPECT_TRUE(m_redirects.empty());
}

TEST_F(SwitchVppDecapHostPath, AFailedRedirectLeavesNoHalfBuiltHostPath)
{
    ASSERT_EQ(SAI_STATUS_SUCCESS, createVlanRif(m_vrB, 3000));

    m_kernelMasters["Vlan3000"] = "Vrft";
    m_redirectStatus = SAI_STATUS_FAILURE;

    vppMock().calls.clear();

    sai_object_id_t nh;

    // the host path is best effort: the tunnel still comes up without one
    ASSERT_EQ(SAI_STATUS_SUCCESS, createNexthop(2000, nh, REMOTE_ROUTER_MAC));

    auto bvi = decapBvi();
    auto pairs = vppCallsTo("configure_lcp_interface");

    // paired, then unpaired again, so no tap is left punting outside the VRF
    ASSERT_EQ(2u, pairs.size());
    EXPECT_EQ(bvi + " tap_" + bvi, pairs[0].name);
    EXPECT_TRUE(pairs[0].flag);
    EXPECT_EQ(bvi + " tap_" + bvi, pairs[1].name);
    EXPECT_FALSE(pairs[1].flag);

    // and removing the next hop has no host path left to tear down
    vppMock().calls.clear();

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->remove(SAI_OBJECT_TYPE_NEXT_HOP, sai_serialize_object_id(nh)));

    EXPECT_TRUE(vppCallsTo("configure_lcp_interface").empty());
}

TEST_F(SwitchVppDecapHostPath, RemoveTearsDownTheHostPath)
{
    ASSERT_EQ(SAI_STATUS_SUCCESS, createVlanRif(m_vrB, 3000));

    m_kernelMasters["Vlan3000"] = "Vrft";

    vppMock().calls.clear();

    sai_object_id_t nh;

    ASSERT_EQ(SAI_STATUS_SUCCESS, createNexthop(2000, nh, REMOTE_ROUTER_MAC));

    auto bvi = decapBvi();

    vppMock().calls.clear();

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->remove(SAI_OBJECT_TYPE_NEXT_HOP, sai_serialize_object_id(nh)));

    int unpair = vppCallIndex("configure_lcp_interface", bvi + " tap_" + bvi);
    int del = vppCallIndex("delete_bvi_interface", bvi);

    ASSERT_NE(-1, unpair);
    ASSERT_NE(-1, del);
    EXPECT_FALSE(vppMock().calls[unpair].flag);
    EXPECT_LT(unpair, del);
    EXPECT_EQ((std::vector<std::string>{"tap_" + bvi}), m_deletedRedirects);
}

TEST_F(SwitchVppDecapHostPath, PortAndSubPortRouterInterfacesResolveKernelVrf)
{
    sai_object_id_t port0 = registerPhysicalPort("TenGigabitEthernet0/0/0", "Ethernet0");
    sai_object_id_t port4 = registerPhysicalPort("TenGigabitEthernet0/0/4", "Ethernet4");
    sai_object_id_t lag2 = registerLag(2);

    // Physical PORT router interface without any VLAN objects in SAI
    auto vrPort = createVr();
    addEncapMapEntry(vrPort, 5001);
    ASSERT_EQ(SAI_STATUS_SUCCESS, createPortRif(vrPort, port0));
    m_kernelMasters["Ethernet0"] = "VrfPort";

    vppMock().calls.clear();
    m_redirects.clear();

    sai_object_id_t nhPort;
    ASSERT_EQ(SAI_STATUS_SUCCESS, createNexthop(5001, nhPort, REMOTE_ROUTER_MAC));
    auto bviPort = decapBvi();
    EXPECT_EQ((std::vector<std::string>{"tap_" + bviPort + " VrfPort"}), m_redirects);

    // Physical SUB_PORT router interface with short Eth<N>.<vlan> kernel netdev
    auto vrEthSub = createVr();
    addEncapMapEntry(vrEthSub, 5002);
    ASSERT_EQ(SAI_STATUS_SUCCESS, createSubPortRif(vrEthSub, port4, 10));
    m_kernelMasters["Eth4.10"] = "VrfEthSub";

    vppMock().calls.clear();
    m_redirects.clear();

    sai_object_id_t nhEthSub;
    ASSERT_EQ(SAI_STATUS_SUCCESS, createNexthop(5002, nhEthSub, REMOTE_ROUTER_MAC));
    auto bviEthSub = decapBvi();
    EXPECT_EQ((std::vector<std::string>{"tap_" + bviEthSub + " VrfEthSub"}), m_redirects);

    // LAG SUB_PORT router interface with short Po<N>.<vlan> kernel netdev
    auto vrLagSub = createVr();
    addEncapMapEntry(vrLagSub, 5003);
    ASSERT_EQ(SAI_STATUS_SUCCESS, createSubPortRif(vrLagSub, lag2, 20));
    m_kernelMasters["Po2.20"] = "VrfLagSub";

    vppMock().calls.clear();
    m_redirects.clear();

    sai_object_id_t nhLagSub;
    ASSERT_EQ(SAI_STATUS_SUCCESS, createNexthop(5003, nhLagSub, REMOTE_ROUTER_MAC));
    auto bviLagSub = decapBvi();
    EXPECT_EQ((std::vector<std::string>{"tap_" + bviLagSub + " VrfLagSub"}), m_redirects);
}

TEST_F(SwitchVppDecapHostPath, LateRouterInterfaceInstallsPendingHostPathsWithoutAggregateRedirect)
{
    sai_object_id_t port0 = registerPhysicalPort("TenGigabitEthernet0/0/0", "Ethernet0");

    vppMock().calls.clear();

    // Both a next hop and a decap term come up before m_vrB has any RIF
    sai_object_id_t nh;
    ASSERT_EQ(SAI_STATUS_SUCCESS, createNexthop(2000, nh, REMOTE_ROUTER_MAC));

    sai_object_id_t decapEntry;
    ASSERT_EQ(SAI_STATUS_SUCCESS, createDecapMapEntry(m_vrB, 4000, &decapEntry));

    auto bvis = vppCallsTo("create_bvi_interface");
    ASSERT_EQ(2u, bvis.size());
    std::string nhBvi = "bvi" + std::to_string(bvis[0].id);
    std::string termBvi = "bvi" + std::to_string(bvis[1].id);

    EXPECT_TRUE(vppCallsTo("configure_lcp_interface").empty());
    EXPECT_TRUE(m_redirects.empty());

    // Now the sub-port RIF arrives in m_vrB enslaved to its kernel VRF device
    m_kernelMasters["Ethernet0.100"] = "Vrft";
    vppMock().calls.clear();

    ASSERT_EQ(SAI_STATUS_SUCCESS, createSubPortRif(m_vrB, port0, 100));

    for (const auto& bvi : {nhBvi, termBvi})
    {
        SCOPED_TRACE(bvi);

        int detach = -1;
        int pair = -1;
        int reattach = -1;
        for (size_t i = 0; i < vppMock().calls.size(); i++)
        {
            const auto& c = vppMock().calls[i];
            if (c.api == "set_sw_interface_l2_bridge" && c.name == bvi)
            {
                if (!c.flag && detach == -1)
                {
                    detach = static_cast<int>(i);
                }
                else if (c.flag && detach != -1)
                {
                    reattach = static_cast<int>(i);
                }
            }
            else if (c.api == "configure_lcp_interface" && c.name == bvi + " tap_" + bvi && c.flag)
            {
                pair = static_cast<int>(i);
            }
        }

        ASSERT_NE(-1, detach);
        ASSERT_NE(-1, pair);
        ASSERT_NE(-1, reattach);
        EXPECT_LT(detach, pair);
        EXPECT_LT(pair, reattach);
    }

    ASSERT_EQ(2u, m_redirects.size());
    EXPECT_EQ("tap_" + nhBvi + " Vrft", m_redirects[0]);
    EXPECT_EQ("tap_" + termBvi + " Vrft", m_redirects[1]);
}

TEST_F(SwitchVppDecapHostPath, DecapTermBviGetsAndTearsDownHostPath)
{
    ASSERT_EQ(SAI_STATUS_SUCCESS, createVlanRif(m_vrB, 3000));
    m_kernelMasters["Vlan3000"] = "Vrft";

    vppMock().calls.clear();
    m_ipv6Disabled.clear();
    m_redirects.clear();
    m_deletedRedirects.clear();

    sai_object_id_t decapEntry;
    ASSERT_EQ(SAI_STATUS_SUCCESS, createDecapMapEntry(m_vrB, 4000, &decapEntry));

    auto bvi = decapBvi();
    int pair = vppCallIndex("configure_lcp_interface", bvi + " tap_" + bvi);
    int bridge = vppCallIndex("set_sw_interface_l2_bridge", bvi);

    ASSERT_NE(-1, pair);
    ASSERT_NE(-1, bridge);
    EXPECT_TRUE(vppMock().calls[pair].flag);
    EXPECT_LT(pair, bridge);
    EXPECT_EQ((std::vector<std::string>{"tap_" + bvi}), m_ipv6Disabled);
    EXPECT_EQ((std::vector<std::string>{"tap_" + bvi + " Vrft"}), m_redirects);

    vppMock().calls.clear();

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->remove(SAI_OBJECT_TYPE_TUNNEL_MAP_ENTRY, sai_serialize_object_id(decapEntry)));

    int unpair = vppCallIndex("configure_lcp_interface", bvi + " tap_" + bvi);
    int del = vppCallIndex("delete_bvi_interface", bvi);

    ASSERT_NE(-1, unpair);
    ASSERT_NE(-1, del);
    EXPECT_FALSE(vppMock().calls[unpair].flag);
    EXPECT_LT(unpair, del);
    EXPECT_EQ((std::vector<std::string>{"tap_" + bvi}), m_deletedRedirects);
}

TEST_F(SwitchVppDecapHostPath, TcRedirectToVrfRewritesPacketTypeToHost)
{
    vppMock().kernelCommands.clear();

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->add_tc_filter_redirect_to_vrf("tap_bvi32768", "Vrft"));
    m_sw->del_tc_filter_redirect("tap_bvi32768");

    ASSERT_EQ(3u, vppMock().kernelCommands.size());
    EXPECT_EQ("tc qdisc add dev tap_bvi32768 clsact", vppMock().kernelCommands[0]);
    EXPECT_EQ("tc filter add dev tap_bvi32768 ingress protocol all prio 10 matchall"
              " action skbedit ptype host pipe action mirred ingress redirect dev Vrft",
              vppMock().kernelCommands[1]);
    EXPECT_EQ("tc qdisc del dev tap_bvi32768 clsact", vppMock().kernelCommands[2]);
}

TEST_F(SwitchVppDecapHostPath, TcRedirectToVrfFallsBackWhenPacketTypeActionIsUnavailable)
{
    vppMock().kernelCommands.clear();
    vppMock().execHook = [](const std::string& cmd, std::string&) {
        SWSS_LOG_ENTER();
        return cmd.find("skbedit") != std::string::npos ? 1 : 0;
    };

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->add_tc_filter_redirect_to_vrf("tap_bvi32768", "Vrft"));

    ASSERT_EQ(3u, vppMock().kernelCommands.size());
    EXPECT_EQ("tc qdisc add dev tap_bvi32768 clsact", vppMock().kernelCommands[0]);
    EXPECT_NE(std::string::npos, vppMock().kernelCommands[1].find("skbedit ptype host"));
    EXPECT_EQ("tc filter add dev tap_bvi32768 ingress protocol all prio 10 u32 match u32 0 0"
              " action mirred ingress redirect dev Vrft",
              vppMock().kernelCommands[2]);

    // When both filter commands fail, the ingress state is cleaned back up
    vppMock().kernelCommands.clear();
    vppMock().execHook = [](const std::string& cmd, std::string&) {
        SWSS_LOG_ENTER();
        return cmd.find("tc filter add") != std::string::npos ? 1 : 0;
    };

    EXPECT_EQ(SAI_STATUS_FAILURE, m_sw->add_tc_filter_redirect_to_vrf("tap_bvi32768", "Vrft"));
    ASSERT_EQ(4u, vppMock().kernelCommands.size());
    EXPECT_EQ("tc qdisc del dev tap_bvi32768 clsact", vppMock().kernelCommands[3]);
}
