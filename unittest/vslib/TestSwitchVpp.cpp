#include "vpp/SwitchVpp.h"

#include "meta/sai_serialize.h"

#include <gtest/gtest.h>

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

    // what sw_interface_feature_enable_disable returns
    int g_vppFeatureRet = 0;

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

// no classify punt in these tests: its one-time setup fails, which the VLAN member path tolerates
int __wrap_vpp_add_node_next(const char *node_name, const char *next_name, uint32_t *next_index) { return -1; }

int __wrap_vpp_classify_set_interface_l2_tables(const char *hwif_name, uint32_t ip4_table_index,
        uint32_t ip6_table_index, uint32_t other_table_index, bool is_input) { return 0; }

int __wrap_vpp_ip_flow_hash_set(uint32_t vrf_id, uint32_t mask, int addr_family) { return 0; }
int __wrap_configure_lcp_interface(const char *hwif_name, const char *hostif_name, bool is_add) { return 0; }

int __wrap_create_bvi_interface(uint8_t *mac_address, uint32_t instance)
{
    SWSS_LOG_ENTER();

    g_vppCalls.push_back({"create_bvi_interface", "bvi" + std::to_string(instance), instance, 0, true});
    return 0;
}

int __wrap_delete_bvi_interface(const char *hwif_name)
{
    SWSS_LOG_ENTER();

    g_vppCalls.push_back({"delete_bvi_interface", hwif_name ? hwif_name : "", 0, 0, false});
    return 0;
}

// recorded with api "<arc>/<feature>" and flag = enable
int __wrap_sw_interface_feature_enable_disable(const char *hwif_name, const char *arc_name,
        const char *feature_name, bool enable)
{
    SWSS_LOG_ENTER();

    g_vppCalls.push_back({std::string(arc_name) + "/" + feature_name, hwif_name ? hwif_name : "", 0, 0, enable});
    return g_vppFeatureRet;
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
 * A VLAN router interface on VLAN 100, which VPP realizes as BVI_HWIF in
 * bridge domain 100. SAI_ROUTER_INTERFACE_ATTR_ADMIN_V4_STATE and
 * ADMIN_V6_STATE turn VPP's ip4-not-enabled and ip6-not-enabled drop features
 * on and off on the BVI.
 */
class SwitchVppBviAdminState : public ::testing::Test
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

            m_vr = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_VIRTUAL_ROUTER, m_switchId);

            sai_attribute_t sattr;

            sattr.id = SAI_SWITCH_ATTR_DEFAULT_VIRTUAL_ROUTER_ID;
            sattr.value.oid = m_vr;

            ASSERT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create_internal(SAI_OBJECT_TYPE_SWITCH, sai_serialize_object_id(m_switchId), m_switchId, 1, &sattr));

            m_vlan = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_VLAN, m_switchId);

            sai_attribute_t vattr;

            vattr.id = SAI_VLAN_ATTR_VLAN_ID;
            vattr.value.u16 = VLAN_ID;

            ASSERT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create_internal(SAI_OBJECT_TYPE_VLAN, sai_serialize_object_id(m_vlan), m_switchId, 1, &vattr));

            m_rif = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_ROUTER_INTERFACE, m_switchId);

            g_vppCalls.clear();
            g_vppAdminUp.clear();
            g_vppFeatureRet = 0;
        }

        void TearDown() override
        {
            m_sw.reset();
        }

        static sai_attribute_t adminState(
                sai_attr_id_t id,
                bool is_up)
        {
            SWSS_LOG_ENTER();

            sai_attribute_t attr;

            attr.id = id;
            attr.value.booldata = is_up;

            return attr;
        }

        // orchagent creates the VLAN router interface, optionally with admin states
        sai_status_t createRif(
                std::vector<sai_attribute_t> admin = {})
        {
            SWSS_LOG_ENTER();

            std::vector<sai_attribute_t> attrs(4);

            attrs[0].id = SAI_ROUTER_INTERFACE_ATTR_TYPE;
            attrs[0].value.s32 = SAI_ROUTER_INTERFACE_TYPE_VLAN;
            attrs[1].id = SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID;
            attrs[1].value.oid = m_vr;
            attrs[2].id = SAI_ROUTER_INTERFACE_ATTR_VLAN_ID;
            attrs[2].value.oid = m_vlan;
            attrs[3].id = SAI_ROUTER_INTERFACE_ATTR_SRC_MAC_ADDRESS;
            memcpy(attrs[3].value.mac, MAC, sizeof(sai_mac_t));

            attrs.insert(attrs.end(), admin.begin(), admin.end());

            return m_sw->createRouterif(m_rif, m_switchId, static_cast<uint32_t>(attrs.size()), attrs.data());
        }

        // what SwitchVpp::set does for a router interface attribute
        void setAdminState(
                sai_attr_id_t id,
                bool is_up)
        {
            SWSS_LOG_ENTER();

            auto attr = adminState(id, is_up);

            EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->vpp_update_router_interface(m_rif, 1, &attr));
        }

        static constexpr uint16_t VLAN_ID = 100;
        static constexpr const char *BVI_HWIF = "bvi100";
        static constexpr const char *IP4_DROP = "ip4-unicast/ip4-not-enabled";
        static constexpr const char *IP6_DROP = "ip6-unicast/ip6-not-enabled";
        static constexpr sai_mac_t MAC = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};

        const sai_object_id_t m_switchId = 0x2100000000;

        std::shared_ptr<RealObjectIdManager> m_mgr;

        std::shared_ptr<SwitchVpp> m_sw;

        sai_object_id_t m_vr = SAI_NULL_OBJECT_ID;
        sai_object_id_t m_vlan = SAI_NULL_OBJECT_ID;
        sai_object_id_t m_rif = SAI_NULL_OBJECT_ID;
};

constexpr uint16_t SwitchVppBviAdminState::VLAN_ID;
constexpr const char *SwitchVppBviAdminState::BVI_HWIF;
constexpr const char *SwitchVppBviAdminState::IP4_DROP;
constexpr const char *SwitchVppBviAdminState::IP6_DROP;
constexpr sai_mac_t SwitchVppBviAdminState::MAC;

TEST_F(SwitchVppBviAdminState, SetDownAndUpTogglesTheDropOncePerChange)
{
    ASSERT_EQ(SAI_STATUS_SUCCESS, createRif());

    ASSERT_EQ(1u, vppCallsTo("create_bvi_interface", BVI_HWIF).size());
    EXPECT_TRUE(vppCallsTo(IP4_DROP, BVI_HWIF).empty());

    setAdminState(SAI_ROUTER_INTERFACE_ATTR_ADMIN_V4_STATE, false);

    auto calls = vppCallsTo(IP4_DROP, BVI_HWIF);

    ASSERT_EQ(1u, calls.size());
    EXPECT_TRUE(calls[0].flag);

    // VPP keeps a copy per enable, so a repeated down must not add another
    setAdminState(SAI_ROUTER_INTERFACE_ATTR_ADMIN_V4_STATE, false);

    EXPECT_EQ(1u, vppCallsTo(IP4_DROP, BVI_HWIF).size());

    setAdminState(SAI_ROUTER_INTERFACE_ATTR_ADMIN_V4_STATE, true);

    calls = vppCallsTo(IP4_DROP, BVI_HWIF);

    ASSERT_EQ(2u, calls.size());
    EXPECT_FALSE(calls[1].flag);

    setAdminState(SAI_ROUTER_INTERFACE_ATTR_ADMIN_V4_STATE, true);

    EXPECT_EQ(2u, vppCallsTo(IP4_DROP, BVI_HWIF).size());
    EXPECT_TRUE(vppCallsTo(IP6_DROP, BVI_HWIF).empty());
}

TEST_F(SwitchVppBviAdminState, V6DownLeavesV4Alone)
{
    ASSERT_EQ(SAI_STATUS_SUCCESS, createRif());

    setAdminState(SAI_ROUTER_INTERFACE_ATTR_ADMIN_V6_STATE, false);

    auto calls = vppCallsTo(IP6_DROP, BVI_HWIF);

    ASSERT_EQ(1u, calls.size());
    EXPECT_TRUE(calls[0].flag);
    EXPECT_TRUE(vppCallsTo(IP4_DROP, BVI_HWIF).empty());
}

/*
 * The admin states are create-and-set attributes: one passed on create was
 * stored without being applied, so the BVI forwarded, and the later up then
 * removed a drop it never added.
 */
TEST_F(SwitchVppBviAdminState, CreatedAdminDownDropsFromTheStart)
{
    ASSERT_EQ(SAI_STATUS_SUCCESS, createRif({
                adminState(SAI_ROUTER_INTERFACE_ATTR_ADMIN_V4_STATE, false),
                adminState(SAI_ROUTER_INTERFACE_ATTR_ADMIN_V6_STATE, true)}));

    auto calls = vppCallsTo(IP4_DROP, BVI_HWIF);

    ASSERT_EQ(1u, calls.size());
    EXPECT_TRUE(calls[0].flag);
    EXPECT_TRUE(vppCallsTo(IP6_DROP, BVI_HWIF).empty());

    setAdminState(SAI_ROUTER_INTERFACE_ATTR_ADMIN_V4_STATE, true);

    calls = vppCallsTo(IP4_DROP, BVI_HWIF);

    ASSERT_EQ(2u, calls.size());
    EXPECT_FALSE(calls[1].flag);
}

TEST_F(SwitchVppBviAdminState, CreateFailsAndRemovesTheBviIfTheDropCannotBeSet)
{
    g_vppFeatureRet = -1;

    EXPECT_NE(SAI_STATUS_SUCCESS, createRif({
                adminState(SAI_ROUTER_INTERFACE_ATTR_ADMIN_V4_STATE, false)}));

    EXPECT_EQ(1u, vppCallsTo("delete_bvi_interface", BVI_HWIF).size());
}
