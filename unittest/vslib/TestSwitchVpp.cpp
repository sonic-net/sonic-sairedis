#include "MockVppXlate.h"
#include "vpp/vppxlate/SaiIntfStats.h"
#include "ContextConfigContainer.h"
#include "meta/DummySaiInterface.h"
#include "meta/Meta.h"
#include "meta/sai_serialize.h"
#include "saivs.h"

#include <gtest/gtest.h>

#include <arpa/inet.h>

#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <unistd.h>

using namespace saivs;

/*
 * Recording stubs for the VPP NAT API. The unit test build links the test
 * binary with the linker's --wrap option for each of them, so SwitchVpp calls
 * land here instead of on a VPP socket. MockVppXlate.cpp stubs the client
 * calls every SwitchVpp makes.
 */
namespace
{
    struct VppCall
    {
        std::string api;
        std::string name;   // interface, or the mapping for a static mapping
        uint32_t id;        // table id, or the session count
        uint32_t sub_id;
        bool flag;          // is_add or enable
    };

    std::vector<VppCall> g_vppCalls;

    // position of the first recorded call matching api and name, or -1
    int vppCallIndex(
            const std::string& api,
            const std::string& name)
    {
        SWSS_LOG_ENTER();

        for (size_t i = 0; i < g_vppCalls.size(); i++)
        {
            if (g_vppCalls[i].api == api && g_vppCalls[i].name == name)
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

        for (auto& call: g_vppCalls)
        {
            if (call.api == api)
            {
                calls.push_back(call);
            }
        }

        return calls;
    }

    bool g_natSupported = true;

    // sessions VPP reports, by inside address (network order)
    std::map<uint32_t, std::vector<vpp_nat44_session_t>> g_natSessions;
}

extern "C" {

bool __wrap_vpp_nat44_ed_supported()
{
    SWSS_LOG_ENTER();

    return g_natSupported;
}

int __wrap_vpp_nat44_ed_enable_disable(bool enable, uint32_t sessions)
{
    SWSS_LOG_ENTER();

    g_vppCalls.push_back({"vpp_nat44_ed_enable_disable", "", sessions, 0, enable});
    return 0;
}

int __wrap_vpp_nat44_forwarding_enable_disable(bool enable)
{
    SWSS_LOG_ENTER();

    g_vppCalls.push_back({"vpp_nat44_forwarding_enable_disable", "", 0, 0, enable});
    return 0;
}

int __wrap_vpp_nat44_ed_output_interface_add_del(const char *hwif_name, bool is_add)
{
    SWSS_LOG_ENTER();

    g_vppCalls.push_back({"vpp_nat44_ed_output_interface_add_del", hwif_name ? hwif_name : "", 0, 0, is_add});
    return 0;
}

int __wrap_vpp_nat44_static_mapping_add_del(const vpp_nat44_static_mapping_t *m, bool is_add)
{
    SWSS_LOG_ENTER();

    char l[INET_ADDRSTRLEN], e[INET_ADDRSTRLEN];

    inet_ntop(AF_INET, &m->local, l, sizeof(l));
    inet_ntop(AF_INET, &m->external, e, sizeof(e));

    // name: "local:port>external:port/protocol" (no ports for an address-only mapping); id: table
    std::string name = m->addr_only
        ? std::string(l) + ">" + e
        : std::string(l) + ":" + std::to_string(m->local_port) + ">" + e + ":" +
          std::to_string(m->external_port) + "/" + std::to_string(m->protocol);

    g_vppCalls.push_back({"vpp_nat44_static_mapping_add_del", name, m->vrf_id, 0, is_add});
    return 0;
}

int __wrap_vpp_nat44_user_sessions_dump(struct in_addr inside_addr, uint32_t vrf_id,
                                        vpp_nat44_session_cb_fn cb, void *ctx)
{
    SWSS_LOG_ENTER();

    for (auto &s: g_natSessions[inside_addr.s_addr])
    {
        cb(&s, ctx);
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

/*
 * A static NAT of a host. Host 10.0.1.3 sits behind a zone 0 router
 * interface, and the VLAN 200 router interface is zone 1. A static NAT of the
 * host to 192.168.1.100 makes orchagent create a SOURCE_NAT entry (source
 * 10.0.1.3 to .100), a DESTINATION_NAT entry (destination .100 to 10.0.1.3)
 * and a DESTINATION_NAT_POOL entry (.100).
 *
 * The unit tests are built with access control checking disabled, so the
 * fixture reaches the protected SwitchVpp members directly.
 */
class SwitchVppNatTest : public ::testing::Test
{
    protected:

        void SetUp() override
        {
            auto sc = std::make_shared<SwitchConfig>(0, "");

            sc->m_saiSwitchType = SAI_SWITCH_TYPE_NPU;
            sc->m_switchType = SAI_VS_SWITCH_TYPE_VPP;
            sc->m_bootType = SAI_VS_BOOT_TYPE_COLD;
            // no tap devices: router interfaces are SAI state only, so the
            // only VPP calls the tests see are the NAT ones
            sc->m_useTapDevice = false;
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
            g_natSupported = true;
            g_natSessions.clear();
        }

        void TearDown() override
        {
            m_sw.reset();
        }

        static uint32_t ip(
                const char *addr)
        {
            SWSS_LOG_ENTER();

            struct in_addr a;

            EXPECT_EQ(1, inet_pton(AF_INET, addr, &a));

            return a.s_addr;
        }

        sai_status_t setSwitch(
                sai_attribute_t attr)
        {
            SWSS_LOG_ENTER();

            return m_sw->set(SAI_OBJECT_TYPE_SWITCH, sai_serialize_object_id(m_switchId), &attr);
        }

        sai_status_t enableNat(
                bool enable)
        {
            SWSS_LOG_ENTER();

            sai_attribute_t attr;

            attr.id = SAI_SWITCH_ATTR_NAT_ENABLE;
            attr.value.booldata = enable;

            return setSwitch(attr);
        }

        sai_status_t createZonedVlanRif(
                uint16_t vlanId,
                uint32_t zone,
                sai_object_id_t *rifOut)
        {
            SWSS_LOG_ENTER();

            auto vlan = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_VLAN, m_switchId);

            sai_attribute_t vattr;

            vattr.id = SAI_VLAN_ATTR_VLAN_ID;
            vattr.value.u16 = vlanId;

            EXPECT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create_internal(SAI_OBJECT_TYPE_VLAN, sai_serialize_object_id(vlan), m_switchId, 1, &vattr));

            sai_attribute_t attrs[5];
            sai_mac_t mac = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x01 };

            attrs[0].id = SAI_ROUTER_INTERFACE_ATTR_TYPE;
            attrs[0].value.s32 = SAI_ROUTER_INTERFACE_TYPE_VLAN;
            attrs[1].id = SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID;
            attrs[1].value.oid = m_defaultVr;
            attrs[2].id = SAI_ROUTER_INTERFACE_ATTR_VLAN_ID;
            attrs[2].value.oid = vlan;
            attrs[3].id = SAI_ROUTER_INTERFACE_ATTR_SRC_MAC_ADDRESS;
            memcpy(attrs[3].value.mac, mac, sizeof(sai_mac_t));
            attrs[4].id = SAI_ROUTER_INTERFACE_ATTR_NAT_ZONE_ID;
            attrs[4].value.u32 = zone;

            *rifOut = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_ROUTER_INTERFACE, m_switchId);

            return m_sw->create(SAI_OBJECT_TYPE_ROUTER_INTERFACE, sai_serialize_object_id(*rifOut), m_switchId, 5, attrs);
        }

        sai_status_t setZone(
                sai_object_id_t rif,
                uint32_t zone)
        {
            SWSS_LOG_ENTER();

            sai_attribute_t attr;

            attr.id = SAI_ROUTER_INTERFACE_ATTR_NAT_ZONE_ID;
            attr.value.u32 = zone;

            return m_sw->set(SAI_OBJECT_TYPE_ROUTER_INTERFACE, sai_serialize_object_id(rif), &attr);
        }

        std::string natEntryId(
                sai_nat_type_t type,
                const char *src,
                const char *dst,
                uint8_t proto = 0,
                uint16_t sport = 0,
                uint16_t dport = 0)
        {
            SWSS_LOG_ENTER();

            sai_nat_entry_t e;

            memset(&e, 0, sizeof(e));
            e.switch_id = m_switchId;
            e.vr_id = m_defaultVr;
            e.nat_type = type;

            if (src)
            {
                e.data.key.src_ip = ip(src);
                e.data.mask.src_ip = 0xffffffff;
            }

            if (dst)
            {
                e.data.key.dst_ip = ip(dst);
                e.data.mask.dst_ip = 0xffffffff;
            }

            if (proto)
            {
                e.data.key.proto = proto;
                e.data.mask.proto = 0xff;
            }

            if (sport)
            {
                e.data.key.l4_src_port = sport;
                e.data.mask.l4_src_port = 0xffff;
            }

            if (dport)
            {
                e.data.key.l4_dst_port = dport;
                e.data.mask.l4_dst_port = 0xffff;
            }

            return sai_serialize_nat_entry(e);
        }

        // the orchagent's SOURCE_NAT entry: SRC_IP, SRC_IP_MASK, counters on
        sai_status_t createSnat(
                const std::string& id,
                const char *translated,
                uint16_t port = 0)
        {
            SWSS_LOG_ENTER();

            std::vector<sai_attribute_t> attrs(4);

            attrs[0].id = SAI_NAT_ENTRY_ATTR_SRC_IP;
            attrs[0].value.ip4 = ip(translated);
            attrs[1].id = SAI_NAT_ENTRY_ATTR_SRC_IP_MASK;
            attrs[1].value.ip4 = 0xffffffff;
            attrs[2].id = SAI_NAT_ENTRY_ATTR_ENABLE_PACKET_COUNT;
            attrs[2].value.booldata = true;
            attrs[3].id = SAI_NAT_ENTRY_ATTR_ENABLE_BYTE_COUNT;
            attrs[3].value.booldata = true;

            if (port)
            {
                sai_attribute_t p;

                p.id = SAI_NAT_ENTRY_ATTR_L4_SRC_PORT;
                p.value.u16 = port;
                attrs.push_back(p);
            }

            return m_sw->create(SAI_OBJECT_TYPE_NAT_ENTRY, id, m_switchId, (uint32_t)attrs.size(), attrs.data());
        }

        sai_status_t createDnat(
                const std::string& id,
                const char *translated)
        {
            SWSS_LOG_ENTER();

            sai_attribute_t attrs[4];

            attrs[0].id = SAI_NAT_ENTRY_ATTR_DST_IP;
            attrs[0].value.ip4 = ip(translated);
            attrs[1].id = SAI_NAT_ENTRY_ATTR_DST_IP_MASK;
            attrs[1].value.ip4 = 0xffffffff;
            attrs[2].id = SAI_NAT_ENTRY_ATTR_ENABLE_PACKET_COUNT;
            attrs[2].value.booldata = true;
            attrs[3].id = SAI_NAT_ENTRY_ATTR_ENABLE_BYTE_COUNT;
            attrs[3].value.booldata = true;

            return m_sw->create(SAI_OBJECT_TYPE_NAT_ENTRY, id, m_switchId, 4, attrs);
        }

        // the three entries orchagent creates for a static NAT of the host
        void createStaticNatEntries()
        {
            SWSS_LOG_ENTER();

            ASSERT_EQ(SAI_STATUS_SUCCESS, createSnat(snatId(), "192.168.1.100"));
            ASSERT_EQ(SAI_STATUS_SUCCESS, createDnat(dnatId(), "10.0.1.3"));
            ASSERT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create(SAI_OBJECT_TYPE_NAT_ENTRY, poolId(), m_switchId, 0, nullptr));
        }

        std::string snatId() { return natEntryId(SAI_NAT_TYPE_SOURCE_NAT, "10.0.1.3", nullptr); }
        std::string dnatId() { return natEntryId(SAI_NAT_TYPE_DESTINATION_NAT, nullptr, "192.168.1.100"); }
        std::string poolId() { return natEntryId(SAI_NAT_TYPE_DESTINATION_NAT_POOL, nullptr, "192.168.1.100"); }

        void addSession(
                const char *inside,
                uint16_t inside_port,
                const char *outside,
                const char *ext_host,
                uint16_t ext_port,
                uint32_t pkts,
                uint64_t bytes)
        {
            SWSS_LOG_ENTER();

            vpp_nat44_session_t s;

            memset(&s, 0, sizeof(s));
            s.inside_addr.s_addr = ip(inside);
            s.inside_port = inside_port;
            s.outside_addr.s_addr = ip(outside);
            s.outside_port = inside_port;
            s.ext_host_addr.s_addr = ip(ext_host);
            s.ext_host_port = ext_port;
            s.protocol = 6;
            s.is_static = true;
            s.total_pkts = pkts;
            s.total_bytes = bytes;

            g_natSessions[s.inside_addr.s_addr].push_back(s);
        }

        uint64_t counter(
                const std::string& id,
                sai_attr_id_t which)
        {
            SWSS_LOG_ENTER();

            sai_attribute_t attr;

            attr.id = which;
            attr.value.u64 = UINT64_MAX;

            EXPECT_EQ(SAI_STATUS_SUCCESS, m_sw->get(SAI_OBJECT_TYPE_NAT_ENTRY, id, 1, &attr));

            return attr.value.u64;
        }

        const sai_object_id_t m_switchId = 0x2100000000;

        std::shared_ptr<RealObjectIdManager> m_mgr;

        std::shared_ptr<SwitchVpp> m_sw;

        sai_object_id_t m_defaultVr = SAI_NULL_OBJECT_ID;
};

TEST_F(SwitchVppNatTest, StaticNatProgramsOneAddressOnlyMapping)
{
    sai_object_id_t rif;

    ASSERT_EQ(SAI_STATUS_SUCCESS, createZonedVlanRif(200, 1, &rif));
    ASSERT_EQ(SAI_STATUS_SUCCESS, enableNat(true));

    g_vppCalls.clear();

    createStaticNatEntries();

    auto maps = vppCallsTo("vpp_nat44_static_mapping_add_del");

    // SOURCE_NAT and DESTINATION_NAT are one mapping; the pool needs nothing
    ASSERT_EQ(1u, maps.size());
    EXPECT_EQ("10.0.1.3>192.168.1.100", maps[0].name);
    EXPECT_EQ(0u, maps[0].id);
    EXPECT_TRUE(maps[0].flag);
}

TEST_F(SwitchVppNatTest, ZoneOneVlanRifIsTheOutsideInterfaceOnceNatIsEnabled)
{
    sai_object_id_t rif;

    ASSERT_EQ(SAI_STATUS_SUCCESS, createZonedVlanRif(200, 1, &rif));

    EXPECT_TRUE(vppCallsTo("vpp_nat44_ed_output_interface_add_del").empty());
    EXPECT_TRUE(vppCallsTo("vpp_nat44_ed_enable_disable").empty());

    ASSERT_EQ(SAI_STATUS_SUCCESS, enableNat(true));

    int en = vppCallIndex("vpp_nat44_ed_enable_disable", "");
    int fwd = vppCallIndex("vpp_nat44_forwarding_enable_disable", "");
    int out = vppCallIndex("vpp_nat44_ed_output_interface_add_del", "bvi200");

    ASSERT_GE(en, 0);
    ASSERT_GE(fwd, 0);
    ASSERT_GE(out, 0);
    EXPECT_TRUE(g_vppCalls[en].flag);
    EXPECT_TRUE(g_vppCalls[fwd].flag);
    EXPECT_TRUE(g_vppCalls[out].flag);
    // forwarding after enable: enabling the plugin resets it
    EXPECT_LT(en, fwd);
}

TEST_F(SwitchVppNatTest, ZoneZeroRouterInterfaceGetsNoNatFeature)
{
    sai_object_id_t rif;

    ASSERT_EQ(SAI_STATUS_SUCCESS, createZonedVlanRif(100, 0, &rif));
    ASSERT_EQ(SAI_STATUS_SUCCESS, enableNat(true));

    EXPECT_TRUE(vppCallsTo("vpp_nat44_ed_output_interface_add_del").empty());
}

TEST_F(SwitchVppNatTest, ZoneSetAfterCreateMovesTheInterfaceInAndOut)
{
    sai_object_id_t rif;

    ASSERT_EQ(SAI_STATUS_SUCCESS, createZonedVlanRif(200, 0, &rif));
    ASSERT_EQ(SAI_STATUS_SUCCESS, enableNat(true));

    g_vppCalls.clear();

    ASSERT_EQ(SAI_STATUS_SUCCESS, setZone(rif, 1));

    auto outs = vppCallsTo("vpp_nat44_ed_output_interface_add_del");

    ASSERT_EQ(1u, outs.size());
    EXPECT_EQ("bvi200", outs[0].name);
    EXPECT_TRUE(outs[0].flag);

    sai_attribute_t attr;

    attr.id = SAI_ROUTER_INTERFACE_ATTR_NAT_ZONE_ID;

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, rif, 1, &attr));
    EXPECT_EQ(1u, attr.value.u32);

    ASSERT_EQ(SAI_STATUS_SUCCESS, setZone(rif, 0));

    outs = vppCallsTo("vpp_nat44_ed_output_interface_add_del");

    ASSERT_EQ(2u, outs.size());
    EXPECT_FALSE(outs[1].flag);
}

TEST_F(SwitchVppNatTest, MappingStaysUntilBothHalvesAreRemoved)
{
    sai_object_id_t rif;

    ASSERT_EQ(SAI_STATUS_SUCCESS, createZonedVlanRif(200, 1, &rif));
    ASSERT_EQ(SAI_STATUS_SUCCESS, enableNat(true));

    createStaticNatEntries();

    g_vppCalls.clear();

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->remove(SAI_OBJECT_TYPE_NAT_ENTRY, snatId()));
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->remove(SAI_OBJECT_TYPE_NAT_ENTRY, poolId()));

    EXPECT_TRUE(vppCallsTo("vpp_nat44_static_mapping_add_del").empty());

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->remove(SAI_OBJECT_TYPE_NAT_ENTRY, dnatId()));

    auto maps = vppCallsTo("vpp_nat44_static_mapping_add_del");

    ASSERT_EQ(1u, maps.size());
    EXPECT_EQ("10.0.1.3>192.168.1.100", maps[0].name);
    EXPECT_FALSE(maps[0].flag);
}

TEST_F(SwitchVppNatTest, EntriesCreatedBeforeNatIsEnabledAreProgrammedWhenItIs)
{
    sai_object_id_t rif;

    ASSERT_EQ(SAI_STATUS_SUCCESS, createZonedVlanRif(200, 1, &rif));

    createStaticNatEntries();

    EXPECT_TRUE(vppCallsTo("vpp_nat44_static_mapping_add_del").empty());

    ASSERT_EQ(SAI_STATUS_SUCCESS, enableNat(true));

    auto maps = vppCallsTo("vpp_nat44_static_mapping_add_del");

    ASSERT_EQ(1u, maps.size());
    EXPECT_TRUE(maps[0].flag);
    // the plugin is up before anything is put in it
    EXPECT_LT(vppCallIndex("vpp_nat44_ed_enable_disable", ""),
              vppCallIndex("vpp_nat44_static_mapping_add_del", "10.0.1.3>192.168.1.100"));
}

TEST_F(SwitchVppNatTest, DisablingNatRemovesEverythingAndTheNextEnableRestoresIt)
{
    sai_object_id_t rif;

    ASSERT_EQ(SAI_STATUS_SUCCESS, createZonedVlanRif(200, 1, &rif));
    ASSERT_EQ(SAI_STATUS_SUCCESS, enableNat(true));

    createStaticNatEntries();

    g_vppCalls.clear();

    ASSERT_EQ(SAI_STATUS_SUCCESS, enableNat(false));

    auto maps = vppCallsTo("vpp_nat44_static_mapping_add_del");
    auto outs = vppCallsTo("vpp_nat44_ed_output_interface_add_del");
    auto en = vppCallsTo("vpp_nat44_ed_enable_disable");

    ASSERT_EQ(1u, maps.size());
    EXPECT_FALSE(maps[0].flag);
    ASSERT_EQ(1u, outs.size());
    EXPECT_FALSE(outs[0].flag);
    ASSERT_EQ(1u, en.size());
    EXPECT_FALSE(en[0].flag);

    g_vppCalls.clear();

    ASSERT_EQ(SAI_STATUS_SUCCESS, enableNat(true));

    EXPECT_EQ(1u, vppCallsTo("vpp_nat44_static_mapping_add_del").size());
    EXPECT_EQ(1u, vppCallsTo("vpp_nat44_ed_output_interface_add_del").size());
}

TEST_F(SwitchVppNatTest, StaticNaptEntryProgramsAPortMapping)
{
    ASSERT_EQ(SAI_STATUS_SUCCESS, enableNat(true));

    auto id = natEntryId(SAI_NAT_TYPE_SOURCE_NAT, "10.0.0.5", nullptr, 6, 2222);

    ASSERT_EQ(SAI_STATUS_SUCCESS, createSnat(id, "192.168.1.100", 22));

    auto maps = vppCallsTo("vpp_nat44_static_mapping_add_del");

    ASSERT_EQ(1u, maps.size());
    EXPECT_EQ("10.0.0.5:2222>192.168.1.100:22/6", maps[0].name);
}

TEST_F(SwitchVppNatTest, SettingTheTranslatedAddressMovesTheMapping)
{
    ASSERT_EQ(SAI_STATUS_SUCCESS, enableNat(true));

    auto id = natEntryId(SAI_NAT_TYPE_SOURCE_NAT, "10.0.0.5", nullptr, 6, 2222);

    ASSERT_EQ(SAI_STATUS_SUCCESS, createSnat(id, "192.168.1.100", 22));

    g_vppCalls.clear();

    sai_attribute_t attr;

    attr.id = SAI_NAT_ENTRY_ATTR_SRC_IP;
    attr.value.ip4 = ip("192.168.1.101");

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->set(SAI_OBJECT_TYPE_NAT_ENTRY, id, &attr));

    auto maps = vppCallsTo("vpp_nat44_static_mapping_add_del");

    // the stored L4_SRC_PORT still applies to the new address
    ASSERT_EQ(2u, maps.size());
    EXPECT_EQ("10.0.0.5:2222>192.168.1.100:22/6", maps[0].name);
    EXPECT_FALSE(maps[0].flag);
    EXPECT_EQ("10.0.0.5:2222>192.168.1.101:22/6", maps[1].name);
    EXPECT_TRUE(maps[1].flag);

    attr.id = SAI_NAT_ENTRY_ATTR_SRC_IP;

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->get(SAI_OBJECT_TYPE_NAT_ENTRY, id, 1, &attr));
    EXPECT_EQ(ip("192.168.1.101"), attr.value.ip4);
}

TEST_F(SwitchVppNatTest, DoubleNatIsStoredButNotProgrammed)
{
    ASSERT_EQ(SAI_STATUS_SUCCESS, enableNat(true));

    auto id = natEntryId(SAI_NAT_TYPE_DOUBLE_NAT, "10.0.0.5", "10.0.0.6");

    sai_attribute_t attrs[2];

    attrs[0].id = SAI_NAT_ENTRY_ATTR_SRC_IP;
    attrs[0].value.ip4 = ip("192.168.2.1");
    attrs[1].id = SAI_NAT_ENTRY_ATTR_DST_IP;
    attrs[1].value.ip4 = ip("192.168.2.2");

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->create(SAI_OBJECT_TYPE_NAT_ENTRY, id, m_switchId, 2, attrs));

    EXPECT_TRUE(vppCallsTo("vpp_nat44_static_mapping_add_del").empty());
}

TEST_F(SwitchVppNatTest, CountersSumTheSessionsOfTheMappingAndStayMonotonic)
{
    sai_object_id_t rif;

    ASSERT_EQ(SAI_STATUS_SUCCESS, createZonedVlanRif(200, 1, &rif));
    ASSERT_EQ(SAI_STATUS_SUCCESS, enableNat(true));

    createStaticNatEntries();

    addSession("10.0.1.3", 40000, "192.168.1.100", "192.168.1.1", 443, 10, 1000);
    addSession("10.0.1.3", 40001, "192.168.1.100", "192.168.1.1", 22, 5, 500);
    // B's traffic that this mapping did not translate (a forwarding bypass session)
    addSession("10.0.1.3", 40002, "10.0.1.3", "10.9.9.9", 80, 100, 10000);

    EXPECT_EQ(15u, counter(snatId(), SAI_NAT_ENTRY_ATTR_PACKET_COUNT));
    EXPECT_EQ(1500u, counter(snatId(), SAI_NAT_ENTRY_ATTR_BYTE_COUNT));
    EXPECT_EQ(15u, counter(dnatId(), SAI_NAT_ENTRY_ATTR_PACKET_COUNT));

    // the :443 session expires and a new one starts
    g_natSessions.clear();
    addSession("10.0.1.3", 40001, "192.168.1.100", "192.168.1.1", 22, 7, 700);
    addSession("10.0.1.3", 40003, "192.168.1.100", "192.168.1.1", 443, 1, 60);

    EXPECT_EQ(18u, counter(snatId(), SAI_NAT_ENTRY_ATTR_PACKET_COUNT));
    EXPECT_EQ(1760u, counter(snatId(), SAI_NAT_ENTRY_ATTR_BYTE_COUNT));

    // orchagent clears with a SET to 0
    sai_attribute_t attr;

    attr.id = SAI_NAT_ENTRY_ATTR_PACKET_COUNT;
    attr.value.u64 = 0;

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->set(SAI_OBJECT_TYPE_NAT_ENTRY, snatId(), &attr));

    EXPECT_EQ(0u, counter(snatId(), SAI_NAT_ENTRY_ATTR_PACKET_COUNT));

    g_natSessions.clear();
    addSession("10.0.1.3", 40001, "192.168.1.100", "192.168.1.1", 22, 9, 900);
    addSession("10.0.1.3", 40003, "192.168.1.100", "192.168.1.1", 443, 1, 60);

    EXPECT_EQ(2u, counter(snatId(), SAI_NAT_ENTRY_ATTR_PACKET_COUNT));
}

TEST_F(SwitchVppNatTest, HitBitIsSetByTrafficAndClearedOnRead)
{
    ASSERT_EQ(SAI_STATUS_SUCCESS, enableNat(true));

    createStaticNatEntries();

    sai_attribute_t attrs[2];

    attrs[0].id = SAI_NAT_ENTRY_ATTR_HIT_BIT;
    attrs[1].id = SAI_NAT_ENTRY_ATTR_HIT_BIT_COR;

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->get(SAI_OBJECT_TYPE_NAT_ENTRY, snatId(), 2, attrs));
    EXPECT_FALSE(attrs[0].value.booldata);

    addSession("10.0.1.3", 40000, "192.168.1.100", "192.168.1.1", 443, 3, 300);

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->get(SAI_OBJECT_TYPE_NAT_ENTRY, snatId(), 2, attrs));
    EXPECT_TRUE(attrs[0].value.booldata);

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->get(SAI_OBJECT_TYPE_NAT_ENTRY, snatId(), 2, attrs));
    EXPECT_FALSE(attrs[0].value.booldata);
}

TEST_F(SwitchVppNatTest, OtherAttributesReadBackAsStored)
{
    createStaticNatEntries();

    sai_attribute_t attr;

    attr.id = SAI_NAT_ENTRY_ATTR_SRC_IP;

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->get(SAI_OBJECT_TYPE_NAT_ENTRY, snatId(), 1, &attr));
    EXPECT_EQ(ip("192.168.1.100"), attr.value.ip4);
}

TEST_F(SwitchVppNatTest, RemovingTheRouterInterfaceDropsItsOutsideFeature)
{
    sai_object_id_t rif;

    ASSERT_EQ(SAI_STATUS_SUCCESS, createZonedVlanRif(200, 1, &rif));
    ASSERT_EQ(SAI_STATUS_SUCCESS, enableNat(true));

    g_vppCalls.clear();

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->remove(SAI_OBJECT_TYPE_ROUTER_INTERFACE, sai_serialize_object_id(rif)));

    auto outs = vppCallsTo("vpp_nat44_ed_output_interface_add_del");

    ASSERT_EQ(1u, outs.size());
    EXPECT_EQ("bvi200", outs[0].name);
    EXPECT_FALSE(outs[0].flag);
}

TEST_F(SwitchVppNatTest, WithoutTheNatPluginNatIsReportedUnsupportedAndNothingIsProgrammed)
{
    g_natSupported = false;

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_AVAILABLE_SNAT_ENTRY;

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->get(SAI_OBJECT_TYPE_SWITCH, sai_serialize_object_id(m_switchId), 1, &attr));
    EXPECT_EQ(0u, attr.value.u32);

    sai_object_id_t rif;

    ASSERT_EQ(SAI_STATUS_SUCCESS, createZonedVlanRif(200, 1, &rif));
    ASSERT_EQ(SAI_STATUS_SUCCESS, enableNat(true));

    createStaticNatEntries();

    EXPECT_TRUE(vppCallsTo("vpp_nat44_ed_enable_disable").empty());
    EXPECT_TRUE(vppCallsTo("vpp_nat44_static_mapping_add_del").empty());

    g_natSupported = true;

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->get(SAI_OBJECT_TYPE_SWITCH, sai_serialize_object_id(m_switchId), 1, &attr));
    EXPECT_NE(0u, attr.value.u32);
}
