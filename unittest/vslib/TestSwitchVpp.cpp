#include "vpp/SwitchVpp.h"

#include "meta/sai_serialize.h"

#include <gtest/gtest.h>

#include <arpa/inet.h>

#include <array>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

using namespace saivs;

/*
 * Recording stubs for the VPP API. The unit test build links the test binary
 * with the linker's --wrap option for each of them, so SwitchVpp calls land
 * here instead of on a VPP socket.
 */
namespace
{
    struct VppCall
    {
        std::string api;
        std::string name;   // interface
        uint32_t id;        // VLAN or bridge domain
        uint32_t sub_id;    // return value
        bool flag;          // is_up or is_add
    };

    std::vector<VppCall> g_vppCalls;

    /*
     * Admin state VPP holds for each interface, by name. An interface that is
     * missing is admin down, which is how VPP creates both ports and
     * sub-interfaces.
     */
    std::map<std::string, bool> g_vppAdminUp;

    bool vppAdminUp(
            const std::string& hwif)
    {
        SWSS_LOG_ENTER();

        auto it = g_vppAdminUp.find(hwif);

        return it != g_vppAdminUp.end() && it->second;
    }

    std::vector<VppCall> vppCallsTo(
            const std::string& api,
            const std::string& name)
    {
        SWSS_LOG_ENTER();

        std::vector<VppCall> calls;

        for (auto& call: g_vppCalls)
        {
            if (call.api == api && call.name == name)
            {
                calls.push_back(call);
            }
        }

        return calls;
    }

    /*
     * The classify tables, sessions and interface bindings SwitchVpp creates,
     * so a test can ask what VPP would do with a given frame. Classify is
     * unavailable (vpp_add_node_next fails) unless a fixture turns it on.
     */
    bool g_classifyAvailable = false;

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

    // (node, next) -> next index, as vlib_node_add_next() hands them out
    std::map<std::pair<std::string, std::string>, uint32_t> g_nodeNexts;
    std::map<std::string, uint32_t> g_nodeNextCount;

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

    uint32_t nodeNext(
            const std::string& node,
            const std::string& next)
    {
        SWSS_LOG_ENTER();

        auto it = g_nodeNexts.find({node, next});

        return it == g_nodeNexts.end() ? MISS - 1 : it->second;
    }

    // IPv4 tables VPP holds, and their API routes: (table, prefix) -> next hop type
    std::set<uint32_t> g_ip4Tables = {0};
    std::map<std::pair<uint32_t, std::string>, int> g_ip4Routes;

    // the IP2ME address set: the destinations the IP2ME policer meters on the punt path
    std::set<std::string> g_ip2meAddrs;
}

extern "C" {

int __wrap_init_vpp_client() { return 0; }
int __wrap_vpp_sync_for_events() { return 0; }
vpp_event_info_t * __wrap_vpp_ev_dequeue() { return NULL; }
int __wrap_vpp_want_l2_macs_events2(bool enable, vpp_mac_event_cb_fn cb, void *ctx) { return 0; }
int __wrap_refresh_interfaces_list() { return 0; }

/*
 * VPP's vnet_sw_interface_set_flags() refuses admin up on a sub-interface whose
 * parent ("super-interface") is admin down, and sw_interface_set_flags returns
 * -1 for it. Admin down is always accepted.
 */
int __wrap_interface_set_state(const char *hwif_name, bool is_up)
{
    SWSS_LOG_ENTER();

    std::string name = hwif_name ? hwif_name : "";

    int ret = 0;

    auto dot = name.rfind('.');

    if (is_up && dot != std::string::npos && !vppAdminUp(name.substr(0, dot)))
    {
        ret = -1;
    }
    else
    {
        g_vppAdminUp[name] = is_up;
    }

    g_vppCalls.push_back({"interface_set_state", name, 0, (uint32_t)ret, is_up});
    return ret;
}

int __wrap_interface_set_promiscuous(const char *hwif_name, bool enable) { return 0; }

int __wrap_create_sub_interface(const char *hwif_name, uint32_t sub_id, uint16_t vlan_id)
{
    SWSS_LOG_ENTER();

    g_vppCalls.push_back({"create_sub_interface", hwif_name ? hwif_name : "", vlan_id, sub_id, true});
    return 0;
}

int __wrap_delete_sub_interface(const char *hwif_name, uint32_t sub_id)
{
    SWSS_LOG_ENTER();

    std::string parent = hwif_name ? hwif_name : "";

    g_vppAdminUp.erase(parent + "." + std::to_string(sub_id));
    g_vppCalls.push_back({"delete_sub_interface", parent, sub_id, sub_id, false});
    return 0;
}

int __wrap_set_sw_interface_l2_bridge(const char *hwif_name, uint32_t bridge_id, bool l2_enable, uint32_t port_type) { return 0; }
int __wrap_set_l2_interface_vlan_tag_rewrite(const char *hwif_name, uint32_t tag1, uint32_t tag2, uint32_t push_dot1q, uint32_t vtr_op) { return 0; }
int __wrap_set_bridge_domain_flags(uint32_t bd_id, vpp_bd_flags_t flag, bool enable) { return 0; }

int __wrap_bridge_domain_get_member_count(uint32_t bd_id, uint32_t *member_count)
{
    SWSS_LOG_ENTER();

    *member_count = 0;
    return 0;
}

int __wrap_vpp_bridge_domain_add_del(uint32_t bridge_id, bool is_add) { return 0; }
uint32_t __wrap_vpp_get_swif_idx_by_name(const char *hwif_name)
{
    SWSS_LOG_ENTER();

    static uint32_t nextSwIfIndex = 200;

    return nextSwIfIndex++;
}

/*
 * Without classify the one-time punt setup fails, which the VLAN member path
 * tolerates; it is retried on the next member.
 */
int __wrap_vpp_add_node_next(const char *node_name, const char *next_name, uint32_t *next_index)
{
    SWSS_LOG_ENTER();

    if (!g_classifyAvailable)
    {
        return -1;
    }

    auto key = std::make_pair(std::string(node_name), std::string(next_name));
    auto it = g_nodeNexts.find(key);

    if (it == g_nodeNexts.end())
    {
        // VPP numbers a node's added next nodes after its static ones; 10 is plenty here
        it = g_nodeNexts.emplace(key, 10 + g_nodeNextCount[node_name]++).first;
    }

    *next_index = it->second;
    return 0;
}

int __wrap_vpp_sonic_ext_feature_get(const char *feature, bool *enabled)
{
    SWSS_LOG_ENTER();

    *enabled = true;
    return 0;
}

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

int __wrap_create_bvi_interface(uint8_t *mac_address, uint32_t instance) { return 0; }
int __wrap_delete_bvi_interface(const char *hwif_name) { return 0; }
int __wrap_configure_lcp_interface(const char *hwif_name, const char *hostif_name, bool is_add) { return 0; }
int __wrap_set_interface_vrf(const char *hwif_name, uint32_t sub_id, uint32_t vrf_id, bool is_ipv6) { return 0; }
int __wrap_vpp_ip_flow_hash_set(uint32_t vrf_id, uint32_t mask, int addr_family) { return 0; }

int __wrap_ip_vrf_add(uint32_t vrf_id, const char *vrf_name, bool is_ipv6)
{
    SWSS_LOG_ENTER();

    g_ip4Tables.insert(vrf_id);
    g_vppCalls.push_back({"ip_vrf_add", vrf_name ? vrf_name : "", vrf_id, 0, true});
    return 0;
}

int __wrap_ip_vrf_del(uint32_t vrf_id, const char *vrf_name, bool is_ipv6)
{
    SWSS_LOG_ENTER();

    g_vppCalls.push_back({"ip_vrf_del", vrf_name ? vrf_name : "", vrf_id, 0, false});

    // VPP ignores a delete of table 0; any other table goes, with its routes
    if (vrf_id != 0)
    {
        for (auto it = g_ip4Routes.begin(); it != g_ip4Routes.end();)
        {
            it = it->first.first == vrf_id ? g_ip4Routes.erase(it) : std::next(it);
        }

        g_ip4Tables.erase(vrf_id);
    }

    return 0;
}

int __wrap_ip_route_add_del(vpp_ip_route_t *route, bool is_add)
{
    SWSS_LOG_ENTER();

    EXPECT_EQ(AF_INET, route->prefix_addr.sa_family);
    EXPECT_EQ(1u, route->nexthop_cnt);

    char addr[INET_ADDRSTRLEN];

    inet_ntop(AF_INET, &route->prefix_addr.addr.ip4.sin_addr, addr, sizeof(addr));

    std::string prefix = std::string(addr) + "/" + std::to_string(route->prefix_len);

    g_vppCalls.push_back({"ip_route_add_del", prefix, route->vrf_id, 0, is_add});

    // VPP has no table to add the route to, or no route to delete
    if (g_ip4Tables.count(route->vrf_id) == 0)
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
            auto sc = std::make_shared<SwitchConfig>(0, "");

            sc->m_saiSwitchType = SAI_SWITCH_TYPE_NPU;
            sc->m_switchType = SAI_VS_SWITCH_TYPE_VPP;
            sc->m_bootType = SAI_VS_BOOT_TYPE_COLD;
            sc->m_useTapDevice = true;
            sc->m_laneMap = LaneMap::getDefaultLaneMap(0);
            sc->m_eventQueue = std::make_shared<EventQueue>(std::make_shared<Signal>());

            auto scc = std::make_shared<SwitchConfigContainer>();

            scc->insert(sc);

            m_mgr = std::make_shared<RealObjectIdManager>(0, scc);
            m_sw = std::make_shared<SwitchVpp>(m_switchId, m_mgr, sc);

            m_vlan = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_VLAN, m_switchId);

            sai_attribute_t vattr;

            vattr.id = SAI_VLAN_ATTR_VLAN_ID;
            vattr.value.u16 = VLAN_ID;

            ASSERT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create_internal(SAI_OBJECT_TYPE_VLAN, sai_serialize_object_id(m_vlan), m_switchId, 1, &vattr));

            addPort(PORT_HWIF, "Ethernet4", m_port, m_bridgePort);
            addPort(OTHER_PORT_HWIF, "Ethernet40", m_otherPort, m_otherBridgePort);

            g_vppCalls.clear();
            g_vppAdminUp.clear();
        }

        void TearDown() override
        {
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

    auto calls = vppCallsTo("interface_set_state", MEMBER_HWIF);

    ASSERT_EQ(1u, calls.size());
    EXPECT_TRUE(calls[0].flag);
    EXPECT_NE(0u, calls[0].sub_id);
    EXPECT_FALSE(vppAdminUp(MEMBER_HWIF));

    setPortAdminState(m_port, true);

    EXPECT_TRUE(vppAdminUp(PORT_HWIF));
    EXPECT_TRUE(vppAdminUp(MEMBER_HWIF));

    calls = vppCallsTo("interface_set_state", MEMBER_HWIF);

    ASSERT_EQ(2u, calls.size());
    EXPECT_TRUE(calls[1].flag);
    EXPECT_EQ(0u, calls[1].sub_id);

    // applied once: a later port flap doesn't set it again
    setPortAdminState(m_port, false);
    setPortAdminState(m_port, true);

    EXPECT_EQ(2u, vppCallsTo("interface_set_state", MEMBER_HWIF).size());
}

TEST_F(SwitchVppSubIfAdminState, TaggedMemberAddedWhileThePortIsUpIsSetUpOnce)
{
    setPortAdminState(m_port, true);

    addTaggedMember(m_bridgePort);

    auto calls = vppCallsTo("interface_set_state", MEMBER_HWIF);

    ASSERT_EQ(1u, calls.size());
    EXPECT_TRUE(calls[0].flag);
    EXPECT_EQ(0u, calls[0].sub_id);
    EXPECT_TRUE(vppAdminUp(MEMBER_HWIF));

    // VPP keeps the sub-interface admin up across a port flap on its own
    setPortAdminState(m_port, false);
    setPortAdminState(m_port, true);

    EXPECT_EQ(1u, vppCallsTo("interface_set_state", MEMBER_HWIF).size());
    EXPECT_TRUE(vppAdminUp(MEMBER_HWIF));
}

TEST_F(SwitchVppSubIfAdminState, TaggedMemberRemovedBeforeThePortComesUpIsNotSetUp)
{
    auto member = addTaggedMember(m_bridgePort);

    EXPECT_FALSE(vppAdminUp(MEMBER_HWIF));

    EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->removeVlanMember(member));

    ASSERT_EQ(1u, vppCallsTo("delete_sub_interface", PORT_HWIF).size());

    g_vppCalls.clear();

    setPortAdminState(m_port, true);

    EXPECT_TRUE(vppAdminUp(PORT_HWIF));
    EXPECT_TRUE(vppCallsTo("interface_set_state", MEMBER_HWIF).empty());
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

    EXPECT_TRUE(vppAdminUp(MEMBER_HWIF));
    EXPECT_FALSE(vppAdminUp(OTHER_MEMBER_HWIF));
    EXPECT_EQ(1u, vppCallsTo("interface_set_state", OTHER_MEMBER_HWIF).size());

    setPortAdminState(m_otherPort, true);

    EXPECT_TRUE(vppAdminUp(OTHER_MEMBER_HWIF));
}

/*
 * The router interface path: SAI_ROUTER_INTERFACE_ATTR_ADMIN_V4_STATE or
 * ADMIN_V6_STATE on a sub-port.
 */
TEST_F(SwitchVppSubIfAdminState, SubPortSetUpWhileThePortIsDownComesUpWithThePort)
{
    setSubPortAdminState(true);

    EXPECT_FALSE(vppAdminUp(SUB_PORT_HWIF));

    setPortAdminState(m_port, true);

    EXPECT_TRUE(vppAdminUp(SUB_PORT_HWIF));
}

TEST_F(SwitchVppSubIfAdminState, SubPortSetDownBeforeThePortComesUpStaysDown)
{
    setSubPortAdminState(true);
    setSubPortAdminState(false);

    auto calls = vppCallsTo("interface_set_state", SUB_PORT_HWIF);

    ASSERT_EQ(2u, calls.size());
    EXPECT_FALSE(calls[1].flag);
    EXPECT_EQ(0u, calls[1].sub_id);

    setPortAdminState(m_port, true);

    EXPECT_EQ(2u, vppCallsTo("interface_set_state", SUB_PORT_HWIF).size());
    EXPECT_FALSE(vppAdminUp(SUB_PORT_HWIF));
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

            g_classifyAvailable = true;
            g_l2Tables.clear();
            g_ip4Tables = {0};
            g_ip4Routes.clear();
            g_ip2meAddrs.clear();
            g_vppCalls.clear();
        }

        void TearDown() override
        {
            m_sw.reset();

            g_classifyAvailable = false;
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

            ASSERT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create_internal(SAI_OBJECT_TYPE_ROUTER_INTERFACE, sai_serialize_object_id(m_rif), m_switchId, 4, attrs));
            ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->vpp_create_bvi_interface(4, attrs));
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

            return nodeNext("l2-input-classify", "linux-cp-punt");
        }

        static uint32_t tagFixup()
        {
            SWSS_LOG_ENTER();

            return nodeNext("l2-input-classify", "sonic-ext-l2-trap-fixup");
        }

        // TRAP: punted on both members, so the BVI never sees it
        void expectTrap()
        {
            SWSS_LOG_ENTER();

            EXPECT_EQ(memberPunt(), onMember("bobm0", false, dhcpOnWire(false)));
            EXPECT_EQ(tagFixup(), onMember("bobm1.100", true, dhcpOnWire(true)));
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

TEST_F(SwitchVppDhcpTrap, NoDhcpTrapKeepsTheTrapBehavior)
{
    addMembers();
    createBvi();

    expectTrap();
}

TEST_F(SwitchVppDhcpTrap, ActionChangesApplyToExistingMembers)
{
    auto trap = createTrap(SAI_PACKET_ACTION_TRAP);
    addMembers();
    createBvi();

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
 * The local 255.255.255.255/32 comes with the table, whatever the DHCP trap
 * action: under TRAP the client broadcast is punted at the member and never
 * reaches the BVI, and any other limited broadcast the BVI gets is received.
 */
TEST_F(SwitchVppDhcpTrap, TheBviTableReceivesLimitedBroadcastsWhateverTheAction)
{
    createTrap(SAI_PACKET_ACTION_TRAP);
    addMembers();

    EXPECT_FALSE(receivesLimitedBroadcast(0));
    EXPECT_FALSE(policesLimitedBroadcast());

    createBvi();

    EXPECT_TRUE(receivesLimitedBroadcast(0));
    EXPECT_TRUE(policesLimitedBroadcast());
    expectTrap();

    // the route stays with the table, not the BVI
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->vpp_delete_bvi_interface(m_rif));

    EXPECT_TRUE(receivesLimitedBroadcast(0));
    EXPECT_TRUE(policesLimitedBroadcast());
}

TEST_F(SwitchVppDhcpTrap, AVrfTableReceivesLimitedBroadcastsUntilItIsRemoved)
{
    auto vrf = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, m_switchId);

    ASSERT_EQ(0, m_sw->vpp_add_ip_vrf(vrf, VRF_TABLE));

    EXPECT_TRUE(receivesLimitedBroadcast(VRF_TABLE));
    EXPECT_FALSE(receivesLimitedBroadcast(0));
    EXPECT_TRUE(policesLimitedBroadcast());

    // a second router interface in the VRF finds it there and adds nothing
    g_vppCalls.clear();

    ASSERT_EQ(0, m_sw->vpp_add_ip_vrf(vrf, VRF_TABLE));

    EXPECT_TRUE(g_vppCalls.empty());

    // the route goes before the table
    ASSERT_EQ(0, m_sw->vpp_del_ip_vrf(vrf));

    ASSERT_EQ(2u, g_vppCalls.size());
    EXPECT_EQ("ip_route_add_del", g_vppCalls[0].api);
    EXPECT_EQ("255.255.255.255/32", g_vppCalls[0].name);
    EXPECT_EQ(VRF_TABLE, g_vppCalls[0].id);
    EXPECT_FALSE(g_vppCalls[0].flag);
    EXPECT_EQ("ip_vrf_del", g_vppCalls[1].api);
    EXPECT_FALSE(receivesLimitedBroadcast(VRF_TABLE));

    // the address set is switch-wide, and other tables still receive limited broadcasts
    EXPECT_TRUE(policesLimitedBroadcast());
}

/*
 * Table 0 outlives every VRF object, so removing one leaves its route in
 * place.
 */
TEST_F(SwitchVppDhcpTrap, TheDefaultTableKeepsItsRouteWhenAVrfObjectGoesAway)
{
    ASSERT_EQ(0, m_sw->vpp_add_ip_vrf(m_defaultVr, 0));

    EXPECT_TRUE(receivesLimitedBroadcast(0));

    ASSERT_EQ(0, m_sw->vpp_del_ip_vrf(m_defaultVr));

    EXPECT_TRUE(receivesLimitedBroadcast(0));
}
