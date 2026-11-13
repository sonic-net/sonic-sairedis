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
#include <tuple>
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

    // IPv4 API routes VPP holds: (table, prefix) -> next hop type. Tests can
    // inject a sequence of API failures before route state is changed.
    std::map<std::pair<uint32_t, std::string>, int> g_ip4Routes;
    std::vector<int> g_ip4RouteResults;

    // the IP2ME address set: the destinations the IP2ME policer meters on the punt path
    std::set<std::string> g_ip2meAddrs;

    // IPv6 multicast routes VPP holds that accept on an interface and forward to
    // local: (table, group/length, accepting interface). Tests can inject a
    // sequence of API results before state is changed.
    std::set<std::tuple<uint32_t, std::string, std::string>> g_ip6LocalMroutes;
    std::vector<int> g_ip6MrouteResults;

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

    if (!g_ip4RouteResults.empty())
    {
        int result = g_ip4RouteResults.front();
        g_ip4RouteResults.erase(g_ip4RouteResults.begin());
        if (result != 0)
        {
            return result;
        }
    }

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

int __wrap_ip6_mroute_local_add_del(uint32_t table_id, const uint8_t grp[16], uint8_t grp_len,
                                    const char *accept_hwif, bool, bool is_add)
{
    SWSS_LOG_ENTER();

    if (!g_ip6MrouteResults.empty())
    {
        int result = g_ip6MrouteResults.front();
        g_ip6MrouteResults.erase(g_ip6MrouteResults.begin());
        if (result != 0)
        {
            return result;
        }
    }

    char addr[INET6_ADDRSTRLEN];

    inet_ntop(AF_INET6, grp, addr, sizeof(addr));

    auto key = std::make_tuple(table_id, std::string(addr) + "/" + std::to_string(grp_len),
                               std::string(accept_hwif));

    if (is_add)
    {
        g_ip6LocalMroutes.insert(key);
        return 0;
    }

    // VPP fails the removal of paths that are not there
    return g_ip6LocalMroutes.erase(key) == 1 ? 0 : -1;
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

            if (u16(0) == 0x3333 && u16(2) == 0x0001 && u16(4) == 0x0002)
            {
                return member + "DHCPv6";
            }

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
            {"untagged DHCPv6", {PUNT}},
            {"untagged ARP", {PUNT}},
            {"untagged IPv6 multicast", {PUNT}},
            {"tagged DHCPv4", {TRAP_FIXUP}},
            {"tagged DHCPv6", {TRAP_FIXUP}},
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
        {"untagged DHCPv6", {TRAP_LEARN}},
        {"untagged ARP", {TRAP_LEARN}},
        {"untagged IPv6 multicast", {TRAP_LEARN}},
        {"tagged DHCPv4", {TRAP_LEARN}},
        {"tagged DHCPv6", {TRAP_LEARN}},
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
        {"untagged DHCPv6", {PUNT}},
        {"untagged ARP", {PUNT}},
        {"untagged IPv6 multicast", {PUNT}},
        {"LLDP", {PUNT}},
        {"VLAN filter", {VLAN_FILTER}},
    });
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
            g_ip4RouteResults.clear();
            g_ip2meAddrs.clear();
            g_ip6LocalMroutes.clear();
            g_ip6MrouteResults.clear();
        }

        void TearDown() override
        {
            m_sw.reset();

            vppMock().reset();
        }

        sai_object_id_t createTrap(
                sai_packet_action_t action,
                sai_hostif_trap_type_t trap_type = SAI_HOSTIF_TRAP_TYPE_DHCP,
                sai_object_id_t trap_group = SAI_NULL_OBJECT_ID)
        {
            SWSS_LOG_ENTER();

            auto oid = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_HOSTIF_TRAP, m_switchId);

            sai_attribute_t attrs[3];

            attrs[0].id = SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE;
            attrs[0].value.s32 = trap_type;
            attrs[1].id = SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION;
            attrs[1].value.s32 = action;
            attrs[2].id = SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP;
            attrs[2].value.oid = trap_group;

            EXPECT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create(SAI_OBJECT_TYPE_HOSTIF_TRAP, sai_serialize_object_id(oid), m_switchId,
                            trap_group == SAI_NULL_OBJECT_ID ? 2 : 3, attrs));

            return oid;
        }

        sai_object_id_t createTrapGroup(
                sai_object_id_t policer)
        {
            SWSS_LOG_ENTER();

            auto group = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_HOSTIF_TRAP_GROUP, m_switchId);
            sai_attribute_t attr;
            attr.id = SAI_HOSTIF_TRAP_GROUP_ATTR_POLICER;
            attr.value.oid = policer;

            EXPECT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create(SAI_OBJECT_TYPE_HOSTIF_TRAP_GROUP,
                            sai_serialize_object_id(group), m_switchId, 1, &attr));
            return group;
        }

        void setTrapGroup(
                sai_object_id_t trap,
                sai_object_id_t group)
        {
            SWSS_LOG_ENTER();

            sai_attribute_t attr;
            attr.id = SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP;
            attr.value.oid = group;

            EXPECT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->set(SAI_OBJECT_TYPE_HOSTIF_TRAP,
                            sai_serialize_object_id(trap), &attr));
        }

        sai_object_id_t addRoutedRif(
                const char *hwif,
                uint32_t table_id)
        {
            SWSS_LOG_ENTER();

            auto port = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_PORT, m_switchId);
            EXPECT_NE(nullptr, m_sw->m_ifaceRegistry.addPhysicalPort(hwif, hwif, port));
            vppMock().interfaceVrfs[hwif] = table_id;

            auto rif = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_ROUTER_INTERFACE, m_switchId);
            sai_attribute_t attrs[3];
            attrs[0].id = SAI_ROUTER_INTERFACE_ATTR_TYPE;
            attrs[0].value.s32 = SAI_ROUTER_INTERFACE_TYPE_PORT;
            attrs[1].id = SAI_ROUTER_INTERFACE_ATTR_PORT_ID;
            attrs[1].value.oid = port;
            attrs[2].id = SAI_ROUTER_INTERFACE_ATTR_ADMIN_V6_STATE;
            attrs[2].value.booldata = true;

            EXPECT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create_internal(SAI_OBJECT_TYPE_ROUTER_INTERFACE,
                            sai_serialize_object_id(rif), m_switchId, 3, attrs));
            return rif;
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

        void createBvi(
                bool admin_v6 = true)
        {
            SWSS_LOG_ENTER();

            m_rif = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_ROUTER_INTERFACE, m_switchId);

            sai_attribute_t attrs[5];

            attrs[0].id = SAI_ROUTER_INTERFACE_ATTR_TYPE;
            attrs[0].value.s32 = SAI_ROUTER_INTERFACE_TYPE_VLAN;
            attrs[1].id = SAI_ROUTER_INTERFACE_ATTR_VLAN_ID;
            attrs[1].value.oid = m_vlan;
            attrs[2].id = SAI_ROUTER_INTERFACE_ATTR_SRC_MAC_ADDRESS;
            memcpy(attrs[2].value.mac, BVI_MAC, sizeof(sai_mac_t));
            attrs[3].id = SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID;
            attrs[3].value.oid = m_defaultVr;
            attrs[4].id = SAI_ROUTER_INTERFACE_ATTR_ADMIN_V6_STATE;
            attrs[4].value.booldata = admin_v6;

            SwitchVpp::RifCreateState state;

            ASSERT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create_internal(SAI_OBJECT_TYPE_ROUTER_INTERFACE, sai_serialize_object_id(m_rif), m_switchId, 5, attrs));
            ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->vpp_create_bvi_interface(5, attrs, state));
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
            bool ip6 = !tagged && frame[12] == 0x86 && frame[13] == 0xdd;

            // an untagged IPv4 or IPv6 frame selects the ip4 or ip6 slot, any
            // tagged frame the other slot
            return classify(ip4 ? it->second[0] : ip6 ? it->second[1] : it->second[2], frame);
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

        // TRAP: punted on both members. An active trap also receives limited
        // broadcasts that arrive on a routed interface; without a trap VPP's
        // default drop remains.
        void expectTrap(
                bool routed_receive = true)
        {
            SWSS_LOG_ENTER();

            EXPECT_EQ(memberPunt(), onMember("bobm0", false, dhcpOnWire(false)));
            EXPECT_EQ(tagFixup(), onMember("bobm1.100", true, dhcpOnWire(true)));
            EXPECT_EQ(routed_receive, receivesLimitedBroadcast(0));
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

    expectTrap(false);
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
    expectTrap(false);

    setTrapAction(trap, SAI_PACKET_ACTION_DROP);
    expectTrap(false);
}

TEST_F(SwitchVppDhcpTrap, RemovingTheCopyTrapRevertsToTrap)
{
    auto trap = createTrap(SAI_PACKET_ACTION_COPY);
    addMembers();
    createBvi();
    expectCopy();

    removeTrap(trap);
    expectTrap(false);
}

TEST_F(SwitchVppDhcpTrap, SemanticBindingFollowsCreateSetAndRemove)
{
    auto policer = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_POLICER, m_switchId);
    auto group = createTrapGroup(policer);
    auto trap = createTrap(SAI_PACKET_ACTION_TRAP);

    ASSERT_EQ(1u, vppMock().coppDhcpBindCalls.size());
    EXPECT_EQ("", vppMock().coppDhcpBindCalls.back().policerName);
    EXPECT_FALSE(vppMock().coppDhcpBindCalls.back().isIpv6);
    EXPECT_TRUE(vppMock().coppDhcpBindCalls.back().isBind);

    setTrapGroup(trap, group);

    ASSERT_EQ(2u, vppMock().coppDhcpBindCalls.size());
    char expected_name[64];
    snprintf(expected_name, sizeof(expected_name), "copp-policer-0x%lx",
            (unsigned long)policer);
    EXPECT_EQ(expected_name, vppMock().coppDhcpBindCalls.back().policerName);
    EXPECT_TRUE(vppMock().coppDhcpBindCalls.back().isBind);

    setTrapAction(trap, SAI_PACKET_ACTION_DROP);
    ASSERT_EQ(3u, vppMock().coppDhcpBindCalls.size());
    EXPECT_FALSE(vppMock().coppDhcpBindCalls.back().isBind);

    setTrapAction(trap, SAI_PACKET_ACTION_COPY);
    ASSERT_EQ(4u, vppMock().coppDhcpBindCalls.size());
    EXPECT_TRUE(vppMock().coppDhcpBindCalls.back().isBind);
    EXPECT_TRUE(vppMock().coppDhcpBindCalls.back().isCopy);

    removeTrap(trap);
    ASSERT_EQ(5u, vppMock().coppDhcpBindCalls.size());
    EXPECT_FALSE(vppMock().coppDhcpBindCalls.back().isBind);
}

TEST_F(SwitchVppDhcpTrap, DhcpSemanticBindingsAreIndependentByFamily)
{
    auto dhcp4 = createTrap(SAI_PACKET_ACTION_TRAP);
    auto dhcp6 = createTrap(SAI_PACKET_ACTION_TRAP, SAI_HOSTIF_TRAP_TYPE_DHCPV6);

    ASSERT_EQ(2u, vppMock().coppDhcpBindCalls.size());
    EXPECT_FALSE(vppMock().coppDhcpBindCalls[0].isIpv6);
    EXPECT_TRUE(vppMock().coppDhcpBindCalls[1].isIpv6);

    removeTrap(dhcp4);
    ASSERT_EQ(3u, vppMock().coppDhcpBindCalls.size());
    EXPECT_FALSE(vppMock().coppDhcpBindCalls.back().isIpv6);
    EXPECT_FALSE(vppMock().coppDhcpBindCalls.back().isBind);

    removeTrap(dhcp6);
    ASSERT_EQ(4u, vppMock().coppDhcpBindCalls.size());
    EXPECT_TRUE(vppMock().coppDhcpBindCalls.back().isIpv6);
    EXPECT_FALSE(vppMock().coppDhcpBindCalls.back().isBind);
}

TEST_F(SwitchVppDhcpTrap, NeighborMissUsesItsSemanticBinding)
{
    auto policer = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_POLICER, m_switchId);
    auto group = createTrapGroup(policer);
    auto trap = createTrap(SAI_PACKET_ACTION_TRAP, SAI_HOSTIF_TRAP_TYPE_NEIGHBOR_MISS);

    ASSERT_EQ(1u, vppMock().coppNeighborMissBindCalls.size());
    EXPECT_EQ("", vppMock().coppNeighborMissBindCalls.back().policerName);
    EXPECT_TRUE(vppMock().coppNeighborMissBindCalls.back().isBind);

    setTrapGroup(trap, group);
    ASSERT_EQ(2u, vppMock().coppNeighborMissBindCalls.size());
    EXPECT_TRUE(vppMock().coppNeighborMissBindCalls.back().isBind);

    removeTrap(trap);
    ASSERT_EQ(3u, vppMock().coppNeighborMissBindCalls.size());
    EXPECT_FALSE(vppMock().coppNeighborMissBindCalls.back().isBind);
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
 * The local 255.255.255.255/32 route is installed while the DHCP trap is
 * active so routed ingress reaches the CPU under either TRAP or COPY. The
 * VLAN-member classifier still distinguishes TRAP from COPY delivery.
 */
TEST_F(SwitchVppDhcpTrap, RouteFailuresRollBackTrapCreateSetAndRemove)
{
    ASSERT_EQ(0, m_sw->vpp_add_ip_vrf(m_defaultVr, 0));

    auto trap = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_HOSTIF_TRAP, m_switchId);
    sai_attribute_t attrs[2]{};
    attrs[0].id = SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE;
    attrs[0].value.s32 = SAI_HOSTIF_TRAP_TYPE_DHCP;
    attrs[1].id = SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION;
    attrs[1].value.s32 = SAI_PACKET_ACTION_TRAP;

    g_ip4RouteResults = {-1};
    EXPECT_EQ(SAI_STATUS_FAILURE,
            m_sw->create(SAI_OBJECT_TYPE_HOSTIF_TRAP, sai_serialize_object_id(trap),
                    m_switchId, 2, attrs));
    EXPECT_FALSE(m_sw->m_dhcp_trap_active);
    EXPECT_FALSE(receivesLimitedBroadcast(0));
    EXPECT_EQ(0u, m_sw->m_trap_map.count(trap));

    ASSERT_EQ(SAI_STATUS_SUCCESS,
            m_sw->create(SAI_OBJECT_TYPE_HOSTIF_TRAP, sai_serialize_object_id(trap),
                    m_switchId, 2, attrs));
    EXPECT_TRUE(m_sw->m_dhcp_trap_active);
    EXPECT_TRUE(receivesLimitedBroadcast(0));

    sai_attribute_t action{};
    action.id = SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION;
    action.value.s32 = SAI_PACKET_ACTION_DROP;
    g_ip4RouteResults = {-1};
    EXPECT_EQ(SAI_STATUS_FAILURE,
            m_sw->set(SAI_OBJECT_TYPE_HOSTIF_TRAP, sai_serialize_object_id(trap), &action));
    EXPECT_TRUE(m_sw->m_dhcp_trap_active);
    EXPECT_TRUE(receivesLimitedBroadcast(0));

    sai_attribute_t stored{};
    stored.id = SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION;
    ASSERT_EQ(SAI_STATUS_SUCCESS,
            m_sw->get(SAI_OBJECT_TYPE_HOSTIF_TRAP, trap, 1, &stored));
    EXPECT_EQ(SAI_PACKET_ACTION_TRAP, stored.value.s32);

    ASSERT_EQ(SAI_STATUS_SUCCESS,
            m_sw->set(SAI_OBJECT_TYPE_HOSTIF_TRAP, sai_serialize_object_id(trap), &action));
    EXPECT_FALSE(m_sw->m_dhcp_trap_active);
    EXPECT_FALSE(receivesLimitedBroadcast(0));

    action.value.s32 = SAI_PACKET_ACTION_TRAP;
    g_ip4RouteResults = {-1};
    EXPECT_EQ(SAI_STATUS_FAILURE,
            m_sw->set(SAI_OBJECT_TYPE_HOSTIF_TRAP, sai_serialize_object_id(trap), &action));
    EXPECT_FALSE(m_sw->m_dhcp_trap_active);
    EXPECT_FALSE(receivesLimitedBroadcast(0));

    ASSERT_EQ(SAI_STATUS_SUCCESS,
            m_sw->set(SAI_OBJECT_TYPE_HOSTIF_TRAP, sai_serialize_object_id(trap), &action));
    EXPECT_TRUE(m_sw->m_dhcp_trap_active);
    EXPECT_TRUE(receivesLimitedBroadcast(0));

    g_ip4RouteResults = {-1};
    EXPECT_EQ(SAI_STATUS_FAILURE,
            m_sw->remove(SAI_OBJECT_TYPE_HOSTIF_TRAP, sai_serialize_object_id(trap)));
    EXPECT_EQ(1u, m_sw->m_trap_map.count(trap));
    EXPECT_TRUE(m_sw->m_dhcp_trap_active);
    EXPECT_TRUE(receivesLimitedBroadcast(0));

    ASSERT_EQ(SAI_STATUS_SUCCESS,
            m_sw->remove(SAI_OBJECT_TYPE_HOSTIF_TRAP, sai_serialize_object_id(trap)));
    EXPECT_FALSE(m_sw->m_dhcp_trap_active);
    EXPECT_FALSE(receivesLimitedBroadcast(0));
}

TEST_F(SwitchVppDhcpTrap, FailedVrfRouteIsNotCachedAndCanRetry)
{
    createTrap(SAI_PACKET_ACTION_TRAP);
    auto vrf = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, m_switchId);

    g_ip4RouteResults = {-1};
    EXPECT_EQ(-1, m_sw->vpp_add_ip_vrf(vrf, VRF_TABLE));
    EXPECT_EQ(0u, m_sw->vrf_objMap.count(vrf));
    // ip_vrf_add() may have created the table or found it already present.
    // The failed caller must not delete shared table state it does not own.
    EXPECT_EQ(1u, vppMock().vrfTables.count(VRF_TABLE));
    EXPECT_FALSE(receivesLimitedBroadcast(VRF_TABLE));

    ASSERT_EQ(0, m_sw->vpp_add_ip_vrf(vrf, VRF_TABLE));
    EXPECT_EQ(1u, m_sw->vrf_objMap.count(vrf));
    EXPECT_TRUE(receivesLimitedBroadcast(VRF_TABLE));
}

TEST_F(SwitchVppDhcpTrap, ActiveTrapReceivesRoutedLimitedBroadcasts)
{
    auto trap = createTrap(SAI_PACKET_ACTION_TRAP);
    addMembers();

    // No IPv4 table has been registered yet, so the route is installed when
    // the BVI/RIF creates that table below.
    EXPECT_FALSE(receivesLimitedBroadcast(0));
    EXPECT_FALSE(policesLimitedBroadcast());

    createBvi();

    expectTrap();
    EXPECT_TRUE(policesLimitedBroadcast());

    setTrapAction(trap, SAI_PACKET_ACTION_COPY);
    expectCopy();

    // the route stays with the table, not the BVI, while the trap is active
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->vpp_delete_bvi_interface(m_rif));

    EXPECT_TRUE(receivesLimitedBroadcast(0));
    EXPECT_TRUE(policesLimitedBroadcast());

    setTrapAction(trap, SAI_PACKET_ACTION_TRAP);

    EXPECT_TRUE(receivesLimitedBroadcast(0));
    EXPECT_TRUE(policesLimitedBroadcast());

    setTrapAction(trap, SAI_PACKET_ACTION_DROP);

    EXPECT_FALSE(receivesLimitedBroadcast(0));
    EXPECT_TRUE(policesLimitedBroadcast());
}

TEST_F(SwitchVppDhcpTrap, AVrfTableReceivesLimitedBroadcastsUntilItIsRemoved)
{
    auto vrf = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, m_switchId);

    // Without an active DHCP trap, a VRF table keeps VPP's default drop while
    // 255.255.255.255 is registered in the IP2ME address set.
    ASSERT_EQ(0, m_sw->vpp_add_ip_vrf(vrf, VRF_TABLE));
    EXPECT_FALSE(receivesLimitedBroadcast(VRF_TABLE));
    EXPECT_TRUE(policesLimitedBroadcast());
    ASSERT_EQ(0, m_sw->vpp_del_ip_vrf(vrf));

    createTrap(SAI_PACKET_ACTION_TRAP);

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
 * place while the DHCP trap is active.
 */
TEST_F(SwitchVppDhcpTrap, TheDefaultTableKeepsItsRouteWhenAVrfObjectGoesAway)
{
    createTrap(SAI_PACKET_ACTION_TRAP);

    ASSERT_EQ(0, m_sw->vpp_add_ip_vrf(m_defaultVr, 0));

    EXPECT_TRUE(receivesLimitedBroadcast(0));

    ASSERT_EQ(0, m_sw->vpp_del_ip_vrf(m_defaultVr));

    EXPECT_TRUE(receivesLimitedBroadcast(0));
}

/*
 * SAI_HOSTIF_TRAP_TYPE_DHCPV6 packet action, on the same VLAN. Each test asks
 * what VPP would do with a DHCPv6 client message, sent to ff02::1:2, on a
 * member (punted by the l2 input classifier, or let through to the flood) and
 * with the BVI's flood copy (received, if the BVI's IPv6 table has ff02::1:2
 * accepting on the BVI and forwarding to local, or dropped).
 */
class SwitchVppDhcp6Trap : public SwitchVppDhcpTrap
{
    protected:

        // an IPv6 UDP packet as it arrives on a member, to group 33:33:<mac_tail>
        static std::vector<uint8_t> ip6UdpOnWire(
                bool tagged,
                const std::array<uint8_t, 4>& mac_tail,
                uint16_t dport)
        {
            SWSS_LOG_ENTER();

            std::vector<uint8_t> f = {0x33, 0x33, mac_tail[0], mac_tail[1], mac_tail[2], mac_tail[3],
                                      0x02, 0, 0, 0, 0, 0x01};

            if (tagged)
            {
                f.insert(f.end(), {0x81, 0x00, 0x00, (uint8_t)VLAN_ID});
            }

            f.insert(f.end(), {0x86, 0xdd});

            std::vector<uint8_t> ip(48, 0);

            ip[0] = 0x60;                                   // IPv6
            ip[5] = 8;                                      // payload length
            ip[6] = 17;                                     // UDP
            ip[7] = 1;                                      // hop limit
            ip[8] = 0xfe; ip[9] = 0x80; ip[23] = 0x01;      // source fe80::1
            ip[24] = 0xff; ip[25] = 0x02;                   // destination ff02::<mac_tail>
            memcpy(&ip[36], mac_tail.data(), mac_tail.size());
            ip[40] = (uint8_t)(546 >> 8);                   // sport 546
            ip[41] = (uint8_t)(546 & 0xff);
            ip[42] = (uint8_t)(dport >> 8);
            ip[43] = (uint8_t)(dport & 0xff);

            f.insert(f.end(), ip.begin(), ip.end());

            return f;
        }

        // a DHCPv6 client message (SOLICIT, REQUEST, ...) to ff02::1:2
        static std::vector<uint8_t> dhcp6OnWire(
                bool tagged)
        {
            SWSS_LOG_ENTER();

            return ip6UdpOnWire(tagged, {0x00, 0x01, 0x00, 0x02}, 547);
        }

        // whether the BVI's flood copy of a DHCPv6 client message is received,
        // in IPv6 table table_id
        static bool bviReceivesDhcp6(
                uint32_t table_id = 0)
        {
            SWSS_LOG_ENTER();

            return g_ip6LocalMroutes.count(std::make_tuple(table_id, std::string("ff02::1:2/128"),
                                                           std::string("bvi100"))) == 1;
        }

        // TRAP: punted on both members, so the BVI never sees it and needs no route
        void expectTrap6()
        {
            SWSS_LOG_ENTER();

            EXPECT_EQ(memberPunt(), onMember("bobm0", false, dhcp6OnWire(false)));
            EXPECT_EQ(tagFixup(), onMember("bobm1.100", true, dhcp6OnWire(true)));
            EXPECT_TRUE(g_ip6LocalMroutes.empty());
        }

        // COPY: flooded on both members, and the BVI's copy received
        void expectCopy6()
        {
            SWSS_LOG_ENTER();

            EXPECT_EQ(MISS, onMember("bobm0", false, dhcp6OnWire(false)));
            EXPECT_EQ(MISS, onMember("bobm1.100", true, dhcp6OnWire(true)));
            EXPECT_TRUE(bviReceivesDhcp6());
        }
};

TEST_F(SwitchVppDhcp6Trap, TrapPuntsOnTheMembersWithNoRoute)
{
    createTrap(SAI_PACKET_ACTION_TRAP, SAI_HOSTIF_TRAP_TYPE_DHCPV6);
    addMembers();
    createBvi();

    expectTrap6();
}

TEST_F(SwitchVppDhcp6Trap, NoDhcp6TrapDefaultsToTrap)
{
    addMembers();
    createBvi();

    expectTrap6();
}

TEST_F(SwitchVppDhcp6Trap, CopyBeforeAnyMemberFloodsAndTheBviCopyIsReceived)
{
    createTrap(SAI_PACKET_ACTION_COPY, SAI_HOSTIF_TRAP_TYPE_DHCPV6);
    addMembers();
    createBvi();

    expectCopy6();
}

TEST_F(SwitchVppDhcp6Trap, SetAttributeSwitchesBetweenTrapAndCopyOnExistingMembersAndBvi)
{
    auto trap = createTrap(SAI_PACKET_ACTION_TRAP, SAI_HOSTIF_TRAP_TYPE_DHCPV6);
    addMembers();
    createBvi();
    expectTrap6();

    setTrapAction(trap, SAI_PACKET_ACTION_COPY);
    expectCopy6();

    setTrapAction(trap, SAI_PACKET_ACTION_TRAP);
    expectTrap6();

    setTrapAction(trap, SAI_PACKET_ACTION_COPY);
    expectCopy6();
}

TEST_F(SwitchVppDhcp6Trap, OtherActionsKeepTheTrapBehavior)
{
    auto trap = createTrap(SAI_PACKET_ACTION_COPY, SAI_HOSTIF_TRAP_TYPE_DHCPV6);
    addMembers();
    createBvi();

    setTrapAction(trap, SAI_PACKET_ACTION_FORWARD);
    expectTrap6();

    setTrapAction(trap, SAI_PACKET_ACTION_DROP);
    expectTrap6();
}

TEST_F(SwitchVppDhcp6Trap, RemovingTheCopyTrapRevertsToTrap)
{
    auto trap = createTrap(SAI_PACKET_ACTION_COPY, SAI_HOSTIF_TRAP_TYPE_DHCPV6);
    addMembers();
    createBvi();
    expectCopy6();

    removeTrap(trap);
    expectTrap6();
}

TEST_F(SwitchVppDhcp6Trap, ReplacementCopyTrapWinsOnlyOnceTheOldTrapIsGone)
{
    auto oldTrap = createTrap(SAI_PACKET_ACTION_TRAP, SAI_HOSTIF_TRAP_TYPE_DHCPV6);
    addMembers();
    createBvi();

    createTrap(SAI_PACKET_ACTION_COPY, SAI_HOSTIF_TRAP_TYPE_DHCPV6);
    expectTrap6();

    removeTrap(oldTrap);
    expectCopy6();
}

TEST_F(SwitchVppDhcp6Trap, BviGetsTheRouteInItsIpv6TableAndLosesItWhenDeleted)
{
    createTrap(SAI_PACKET_ACTION_COPY, SAI_HOSTIF_TRAP_TYPE_DHCPV6);

    // the BVI's IPv6 table, whatever its router interface's VRF
    vppMock().interfaceVrfs["bvi100"] = 7;

    createBvi();

    EXPECT_TRUE(bviReceivesDhcp6(7));
    EXPECT_EQ(1u, g_ip6LocalMroutes.size());

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->vpp_delete_bvi_interface(m_rif));

    EXPECT_TRUE(g_ip6LocalMroutes.empty());
}

TEST_F(SwitchVppDhcp6Trap, RoutedReceivePersistsAcrossTrapCopyTransitions)
{
    // Share table 0 with the BVI so the route keeps its local Forward path
    // while individual Accept interfaces come and go.
    addRoutedRif("Ethernet42", 0);
    auto trap = createTrap(SAI_PACKET_ACTION_TRAP, SAI_HOSTIF_TRAP_TYPE_DHCPV6);

    EXPECT_EQ(1u, g_ip6LocalMroutes.count(
            std::make_tuple(0u, std::string("ff02::1:2/128"), std::string("Ethernet42"))));

    createBvi();
    EXPECT_FALSE(bviReceivesDhcp6());

    setTrapAction(trap, SAI_PACKET_ACTION_COPY);
    EXPECT_TRUE(bviReceivesDhcp6());
    EXPECT_EQ(1u, g_ip6LocalMroutes.count(
            std::make_tuple(0u, std::string("ff02::1:2/128"), std::string("Ethernet42"))));

    setTrapAction(trap, SAI_PACKET_ACTION_TRAP);
    EXPECT_FALSE(bviReceivesDhcp6());
    EXPECT_EQ(1u, g_ip6LocalMroutes.count(
            std::make_tuple(0u, std::string("ff02::1:2/128"), std::string("Ethernet42"))));

    removeTrap(trap);
    EXPECT_TRUE(g_ip6LocalMroutes.empty());
}

TEST_F(SwitchVppDhcp6Trap, ReceiveRouteFailuresRollBackCreateSetAndRemove)
{
    addMembers();
    createBvi();

    auto trap = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_HOSTIF_TRAP, m_switchId);
    sai_attribute_t attrs[2];
    attrs[0].id = SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE;
    attrs[0].value.s32 = SAI_HOSTIF_TRAP_TYPE_DHCPV6;
    attrs[1].id = SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION;
    attrs[1].value.s32 = SAI_PACKET_ACTION_COPY;

    g_ip6MrouteResults = {-1};
    EXPECT_EQ(SAI_STATUS_FAILURE,
            m_sw->create(SAI_OBJECT_TYPE_HOSTIF_TRAP, sai_serialize_object_id(trap),
                    m_switchId, 2, attrs));
    EXPECT_EQ(0u, m_sw->m_trap_map.count(trap));
    auto stored_traps = m_sw->m_objectHash.find(SAI_OBJECT_TYPE_HOSTIF_TRAP);
    EXPECT_TRUE(stored_traps == m_sw->m_objectHash.end() ||
            stored_traps->second.count(sai_serialize_object_id(trap)) == 0);
    expectTrap6();

    ASSERT_EQ(SAI_STATUS_SUCCESS,
            m_sw->create(SAI_OBJECT_TYPE_HOSTIF_TRAP, sai_serialize_object_id(trap),
                    m_switchId, 2, attrs));
    expectCopy6();

    sai_attribute_t action{};
    action.id = SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION;
    action.value.s32 = SAI_PACKET_ACTION_TRAP;
    g_ip6MrouteResults = {-1};
    EXPECT_EQ(SAI_STATUS_FAILURE,
            m_sw->set(SAI_OBJECT_TYPE_HOSTIF_TRAP, sai_serialize_object_id(trap), &action));
    expectCopy6();

    sai_attribute_t stored{};
    stored.id = SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION;
    ASSERT_EQ(SAI_STATUS_SUCCESS,
            m_sw->get(SAI_OBJECT_TYPE_HOSTIF_TRAP, trap, 1, &stored));
    EXPECT_EQ(SAI_PACKET_ACTION_COPY, stored.value.s32);

    ASSERT_EQ(SAI_STATUS_SUCCESS,
            m_sw->set(SAI_OBJECT_TYPE_HOSTIF_TRAP, sai_serialize_object_id(trap), &action));
    expectTrap6();

    action.value.s32 = SAI_PACKET_ACTION_COPY;
    ASSERT_EQ(SAI_STATUS_SUCCESS,
            m_sw->set(SAI_OBJECT_TYPE_HOSTIF_TRAP, sai_serialize_object_id(trap), &action));
    expectCopy6();

    g_ip6MrouteResults = {-1};
    EXPECT_EQ(SAI_STATUS_FAILURE,
            m_sw->remove(SAI_OBJECT_TYPE_HOSTIF_TRAP, sai_serialize_object_id(trap)));
    EXPECT_EQ(1u, m_sw->m_trap_map.count(trap));
    expectCopy6();

    ASSERT_EQ(SAI_STATUS_SUCCESS,
            m_sw->remove(SAI_OBJECT_TYPE_HOSTIF_TRAP, sai_serialize_object_id(trap)));
    expectTrap6();
}

TEST_F(SwitchVppDhcp6Trap, AdminDownBviDoesNotReceiveDhcp6Copy)
{
    createTrap(SAI_PACKET_ACTION_COPY, SAI_HOSTIF_TRAP_TYPE_DHCPV6);
    addMembers();
    createBvi(false);

    EXPECT_FALSE(bviReceivesDhcp6());
}

TEST_F(SwitchVppDhcp6Trap, OnlyDhcp6IsPuntedAmongIpv6Multicast)
{
    addMembers();

    // all-nodes multicast is flooded as before
    EXPECT_EQ(MISS, onMember("bobm0", false, ip6UdpOnWire(false, {0x00, 0x00, 0x00, 0x01}, 547)));
    EXPECT_EQ(MISS, onMember("bobm1.100", true, ip6UdpOnWire(true, {0x00, 0x00, 0x00, 0x01}, 547)));
}

TEST_F(SwitchVppDhcp6Trap, Dhcp6AndDhcp4ActionsAreIndependent)
{
    createTrap(SAI_PACKET_ACTION_TRAP);
    createTrap(SAI_PACKET_ACTION_COPY, SAI_HOSTIF_TRAP_TYPE_DHCPV6);
    addMembers();
    createBvi();

    expectCopy6();
    expectTrap();

    // broadcast ARP is still punted on the tagged member, whose chain ends in the DHCPv6 table
    EXPECT_EQ(tagFixup(), onMember("bobm1.100", true, arpOnWire(true)));
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

    // VPP's L2FIB has the MAC on the local port now
    vpp_l2fib_entry_t row = {};

    memcpy(row.mac, REMOTE_MAC, sizeof(row.mac));
    row.bd_id = VLAN_ID;
    row.sw_if_index = BOND_SW_IF_INDEX;

    vppMock().l2fib = {row};

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
