#include "vpp/SwitchVpp.h"

#include "meta/sai_serialize.h"

#include <gtest/gtest.h>

#include <arpa/inet.h>

#include <array>
#include <cstring>
#include <map>
#include <memory>
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
        std::string name;   // interface, table name or MAC
        uint32_t id;        // VLAN, bridge domain, table id or VNI
        uint32_t sub_id;    // return value, sub-interface, port type or decap_any
        bool flag;          // is_up, is_add or is_ipv6
    };

    std::vector<VppCall> g_vppCalls;

    uint32_t g_nextTunnelSwIfIndex = 100;

    std::string macStr(
            const uint8_t *mac)
    {
        SWSS_LOG_ENTER();

        char buf[18];

        snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

        return buf;
    }

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

    std::vector<VppCall> vppCallsTo(
            const std::string& api)
    {
        SWSS_LOG_ENTER();

        std::vector<VppCall> calls;

        for (auto& call: g_vppCalls)
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

int __wrap_set_sw_interface_l2_bridge(const char *hwif_name, uint32_t bridge_id, bool l2_enable, uint32_t port_type)
{
    SWSS_LOG_ENTER();

    g_vppCalls.push_back({"set_sw_interface_l2_bridge", hwif_name ? hwif_name : "", bridge_id, port_type, l2_enable});
    return 0;
}

int __wrap_set_sw_interface_l2_bridge_by_index(uint32_t sw_if_index, uint32_t bridge_id, bool l2_enable, uint32_t port_type) { return 0; }
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

// no classify punt in these tests: its one-time setup fails, which the VLAN member path tolerates
int __wrap_vpp_add_node_next(const char *node_name, const char *next_name, uint32_t *next_index) { return -1; }

int __wrap_vpp_classify_set_interface_l2_tables(const char *hwif_name, uint32_t ip4_table_index,
        uint32_t ip6_table_index, uint32_t other_table_index, bool is_input) { return 0; }

int __wrap_sw_interface_set_mac_by_index(uint32_t sw_if_index, uint8_t *mac_address) { return 0; }

int __wrap_configure_lcp_interface(const char *hwif_name, const char *hostif_name, bool is_add)
{
    SWSS_LOG_ENTER();

    g_vppCalls.push_back({"configure_lcp_interface",
            std::string(hwif_name ? hwif_name : "") + " " + (hostif_name ? hostif_name : ""), 0, 0, is_add});
    return 0;
}

int __wrap_interface_ip_address_add_del(const char *hw_ifname, vpp_ip_route_t *prefix, bool is_add) { return 0; }

int __wrap_ip4_nbr_add_del(const char *hwif_name, uint32_t sw_if_index, struct sockaddr_in *addr,
        bool is_static, bool no_fib_entry, uint8_t *mac, bool is_add)
{
    SWSS_LOG_ENTER();

    g_vppCalls.push_back({"ip4_nbr_add_del", macStr(mac), sw_if_index, 0, is_add});
    return 0;
}

int __wrap_ip6_nbr_add_del(const char *hwif_name, uint32_t sw_if_index, struct sockaddr_in6 *addr,
        bool is_static, bool no_fib_entry, uint8_t *mac, bool is_add) { return 0; }

int __wrap_ip_vrf_add(uint32_t vrf_id, const char *vrf_name, bool is_ipv6)
{
    SWSS_LOG_ENTER();

    g_vppCalls.push_back({"ip_vrf_add", vrf_name ? vrf_name : "", vrf_id, 0, is_ipv6});
    return 0;
}

int __wrap_ip_vrf_del(uint32_t vrf_id, const char *vrf_name, bool is_ipv6)
{
    SWSS_LOG_ENTER();

    g_vppCalls.push_back({"ip_vrf_del", vrf_name ? vrf_name : "", vrf_id, 0, is_ipv6});
    return 0;
}

int __wrap_vpp_ip_flow_hash_set(uint32_t vrf_id, uint32_t mask, int addr_family)
{
    SWSS_LOG_ENTER();

    g_vppCalls.push_back({"vpp_ip_flow_hash_set", "", vrf_id, 0, addr_family == AF_INET6});
    return 0;
}

int __wrap_set_interface_vrf(const char *hwif_name, uint32_t sub_id, uint32_t vrf_id, bool is_ipv6)
{
    SWSS_LOG_ENTER();

    g_vppCalls.push_back({"set_interface_vrf", hwif_name ? hwif_name : "", vrf_id, sub_id, is_ipv6});
    return 0;
}

int __wrap_create_bvi_interface(uint8_t *mac_address, uint32_t instance)
{
    SWSS_LOG_ENTER();

    g_vppCalls.push_back({"create_bvi_interface", macStr(mac_address), instance, 0, true});
    return 0;
}

int __wrap_delete_bvi_interface(const char *hwif_name)
{
    SWSS_LOG_ENTER();

    g_vppCalls.push_back({"delete_bvi_interface", hwif_name ? hwif_name : "", 0, 0, false});
    return 0;
}

int __wrap_vpp_vxlan_tunnel_add_del(vpp_vxlan_tunnel_t *tunnel, bool is_add, uint32_t *sw_if_index)
{
    SWSS_LOG_ENTER();

    g_vppCalls.push_back({"vpp_vxlan_tunnel_add_del", "", tunnel->vni, tunnel->decap_any ? 1u : 0u, is_add});

    if (is_add)
    {
        *sw_if_index = g_nextTunnelSwIfIndex++;
    }

    return 0;
}

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
            sc->m_useTapDevice = true;
            sc->m_laneMap = LaneMap::getDefaultLaneMap(0);
            sc->m_eventQueue = std::make_shared<EventQueue>(std::make_shared<Signal>());

            auto scc = std::make_shared<SwitchConfigContainer>();

            scc->insert(sc);

            m_mgr = std::make_shared<RealObjectIdManager>(0, scc);
            m_sw = std::make_shared<SwitchVpp>(m_switchId, m_mgr, sc);

            ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->create_default_virtual_router());

            sai_attribute_t attr;

            attr.id = SAI_SWITCH_ATTR_DEFAULT_VIRTUAL_ROUTER_ID;

            ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->get(SAI_OBJECT_TYPE_SWITCH, sai_serialize_object_id(m_switchId), 1, &attr));

            m_defaultVr = attr.value.oid;

            g_vppCalls.clear();
        }

        void TearDown() override
        {
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

    g_vppCalls.clear();

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

    g_vppCalls.clear();

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

    g_vppCalls.clear();

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

            g_vppCalls.clear();
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

        // what orchagent creates for the VNI of a VRF, after the tunnel
        sai_status_t createDecapMapEntry(
                sai_object_id_t vr,
                uint32_t vni)
        {
            SWSS_LOG_ENTER();

            auto entry = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_TUNNEL_MAP_ENTRY, m_switchId);

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
    EXPECT_EQ(1u, tunnels[0].sub_id);
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
