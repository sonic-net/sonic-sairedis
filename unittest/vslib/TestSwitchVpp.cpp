#include "vpp/SwitchVpp.h"

#include "meta/sai_serialize.h"

#include <gtest/gtest.h>

#include <array>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace saivs;

/*
 * Stubs for the VPP API. The unit test build links the test binary with the
 * linker's --wrap option for each of them, so SwitchVpp calls land here
 * instead of on a VPP socket. The classify stubs keep the tables, sessions
 * and interface bindings SwitchVpp creates, so a test can ask what VPP would
 * do with a given frame.
 */
namespace
{
    struct ClassifyTable
    {
        uint32_t skip;
        uint32_t match;
        uint32_t next_table;
        bool use_current_data;
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

    // interface -> ip4 input ACL table
    std::map<std::string, uint32_t> g_ip4InputAcl;

    std::vector<std::string> g_bviCalls;

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
}

extern "C" {

int __wrap_init_vpp_client() { return 0; }
int __wrap_vpp_sync_for_events() { return 0; }
vpp_event_info_t * __wrap_vpp_ev_dequeue() { return NULL; }
int __wrap_vpp_want_l2_macs_events2(bool enable, vpp_mac_event_cb_fn cb, void *ctx) { return 0; }
int __wrap_refresh_interfaces_list() { return 0; }
uint32_t __wrap_vpp_get_swif_idx_by_name(const char *hwif_name) { return 100; }
int __wrap_set_sw_interface_l2_bridge(const char *hwif_name, uint32_t bridge_id, bool l2_enable, uint32_t port_type) { return 0; }
int __wrap_create_sub_interface(const char *hwif_name, uint32_t sub_id, uint16_t vlan_id) { return 0; }
int __wrap_set_l2_interface_vlan_tag_rewrite(const char *hwif_name, uint32_t tag1, uint32_t tag2, uint32_t push_dot1q, uint32_t vtr_op) { return 0; }
int __wrap_interface_set_state(const char *hwif_name, bool is_up) { return 0; }
int __wrap_set_bridge_domain_flags(uint32_t bd_id, vpp_bd_flags_t flag, bool enable) { return 0; }
int __wrap_configure_lcp_interface(const char *hwif_name, const char *hostif_name, bool is_add) { return 0; }

int __wrap_vpp_sonic_ext_feature_get(const char *feature, bool *enabled)
{
    SWSS_LOG_ENTER();

    *enabled = true;
    return 0;
}

int __wrap_create_bvi_interface(uint8_t *mac_address, uint32_t instance)
{
    SWSS_LOG_ENTER();

    g_bviCalls.push_back("create bvi" + std::to_string(instance));
    return 0;
}

int __wrap_delete_bvi_interface(const char *hwif_name)
{
    SWSS_LOG_ENTER();

    // VPP drops an interface's input ACL with the interface; record what was left
    std::string name = hwif_name;

    g_bviCalls.push_back("delete " + name + (g_ip4InputAcl.count(name) ? " with acl" : ""));
    return 0;
}

int __wrap_vpp_add_node_next(const char *node_name, const char *next_name, uint32_t *next_index)
{
    SWSS_LOG_ENTER();

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

int __wrap_vpp_classify_table_create_ex(uint32_t nbuckets, uint32_t memory_size,
                                        uint32_t skip_n_vectors, uint32_t match_n_vectors,
                                        uint32_t next_table_index, uint32_t miss_next_index,
                                        bool use_current_data, int16_t current_data_offset,
                                        const uint8_t *mask, uint32_t mask_len,
                                        uint32_t *new_table_index)
{
    SWSS_LOG_ENTER();

    EXPECT_EQ(match_n_vectors * 16, mask_len);

    ClassifyTable t;

    t.skip = skip_n_vectors;
    t.match = match_n_vectors;
    t.next_table = next_table_index;
    t.use_current_data = use_current_data;
    t.mask.assign(mask, mask + mask_len);

    *new_table_index = g_nextTableIndex++;
    g_tables[*new_table_index] = t;
    return 0;
}

int __wrap_vpp_classify_table_create(uint32_t nbuckets, uint32_t memory_size,
                                     uint32_t skip_n_vectors, uint32_t match_n_vectors,
                                     uint32_t next_table_index, uint32_t miss_next_index,
                                     const uint8_t *mask, uint32_t mask_len,
                                     uint32_t *new_table_index)
{
    SWSS_LOG_ENTER();

    return __wrap_vpp_classify_table_create_ex(nbuckets, memory_size, skip_n_vectors, match_n_vectors,
            next_table_index, miss_next_index, false, 0, mask, mask_len, new_table_index);
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
                                                uint32_t ip6_table_index, uint32_t other_table_index,
                                                bool is_input)
{
    SWSS_LOG_ENTER();

    g_l2Tables[hwif_name] = {ip4_table_index, ip6_table_index, other_table_index};
    return 0;
}

int __wrap_vpp_input_acl_set_interface(const char *hwif_name, uint32_t ip4_table_index, bool is_add)
{
    SWSS_LOG_ENTER();

    if (is_add)
    {
        g_ip4InputAcl[hwif_name] = ip4_table_index;
        return 0;
    }

    // VPP rejects the unbind of a table that is not bound
    auto it = g_ip4InputAcl.find(hwif_name);

    if (it == g_ip4InputAcl.end() || it->second != ip4_table_index)
    {
        return -1;
    }

    g_ip4InputAcl.erase(it);
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
 * SAI_HOSTIF_TRAP_TYPE_DHCP packet action on VLAN 100: an untagged member on
 * one port, a tagged one on another, and the VLAN's BVI. Each test asks what
 * VPP would do with a DHCPv4 client broadcast on a member (punted by the l2
 * input classifier, or let through to the flood) and with the BVI's flood
 * copy (punted by the BVI's ip4 input ACL, or left to ip4-lookup, where
 * 255.255.255.255 is a drop route).
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

            g_l2Tables.clear();
            g_ip4InputAcl.clear();
            g_bviCalls.clear();
        }

        void TearDown() override
        {
            m_sw.reset();
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

            sai_attribute_t attrs[3];

            attrs[0].id = SAI_ROUTER_INTERFACE_ATTR_TYPE;
            attrs[0].value.s32 = SAI_ROUTER_INTERFACE_TYPE_VLAN;
            attrs[1].id = SAI_ROUTER_INTERFACE_ATTR_VLAN_ID;
            attrs[1].value.oid = m_vlan;
            attrs[2].id = SAI_ROUTER_INTERFACE_ATTR_SRC_MAC_ADDRESS;
            memcpy(attrs[2].value.mac, BVI_MAC, sizeof(sai_mac_t));

            ASSERT_EQ(SAI_STATUS_SUCCESS,
                    m_sw->create_internal(SAI_OBJECT_TYPE_ROUTER_INTERFACE, sai_serialize_object_id(m_rif), m_switchId, 3, attrs));
            ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->vpp_create_bvi_interface(3, attrs));
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

        // the BVI input ACL's verdict on the BVI's flood copy (matched from the IP header)
        static uint32_t onBvi(
                const std::vector<uint8_t>& ip)
        {
            SWSS_LOG_ENTER();

            auto it = g_ip4InputAcl.find(BVI);

            if (it == g_ip4InputAcl.end())
            {
                return MISS;
            }

            EXPECT_TRUE(g_tables.at(it->second).use_current_data);

            return classify(it->second, ip);
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

        static uint32_t bviPunt()
        {
            SWSS_LOG_ENTER();

            return nodeNext("ip4-inacl", "linux-cp-punt");
        }

        // TRAP: punted on both members, nothing at the BVI
        void expectTrap()
        {
            SWSS_LOG_ENTER();

            EXPECT_EQ(memberPunt(), onMember("bobm0", false, dhcpOnWire(false)));
            EXPECT_EQ(tagFixup(), onMember("bobm1.100", true, dhcpOnWire(true)));
            EXPECT_EQ(0u, g_ip4InputAcl.count(BVI));
        }

        // COPY: flooded on both members, the BVI copy punted
        void expectCopy()
        {
            SWSS_LOG_ENTER();

            EXPECT_EQ(MISS, onMember("bobm0", false, dhcpOnWire(false)));
            EXPECT_EQ(MISS, onMember("bobm1.100", true, dhcpOnWire(true)));
            EXPECT_EQ(bviPunt(), onBvi(ip4Udp(0xff, 67)));
        }

        static constexpr uint16_t VLAN_ID = 100;
        static constexpr const char *BVI = "bvi100";
        static constexpr uint8_t BVI_MAC[6] = { 0x02, 0x00, 0x00, 0x00, 0x01, 0x00 };

        const sai_object_id_t m_switchId = 0x2100000000;

        std::shared_ptr<RealObjectIdManager> m_mgr;

        std::shared_ptr<SwitchVpp> m_sw;

        sai_object_id_t m_vlan = SAI_NULL_OBJECT_ID;
        sai_object_id_t m_rif = SAI_NULL_OBJECT_ID;
};

constexpr uint16_t SwitchVppDhcpTrap::VLAN_ID;
constexpr const char *SwitchVppDhcpTrap::BVI;
constexpr uint8_t SwitchVppDhcpTrap::BVI_MAC[6];

// Runs first in this binary, so it also covers a COPY trap that predates the classify tables.
TEST_F(SwitchVppDhcpTrap, CopyBeforeAnyMemberFloodsAndPuntsTheBviCopy)
{
    createTrap(SAI_PACKET_ACTION_COPY);
    addMembers();
    createBvi();

    expectCopy();
}

TEST_F(SwitchVppDhcpTrap, TrapPuntsOnTheMembersAndLeavesTheBviAlone)
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

TEST_F(SwitchVppDhcpTrap, ActionChangesApplyToExistingMembersAndBvi)
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

TEST_F(SwitchVppDhcpTrap, BviCreatedWhileCopyGetsTheAcl)
{
    auto trap = createTrap(SAI_PACKET_ACTION_TRAP);
    addMembers();

    setTrapAction(trap, SAI_PACKET_ACTION_COPY);
    EXPECT_EQ(0u, g_ip4InputAcl.count(BVI));

    createBvi();
    expectCopy();
}

TEST_F(SwitchVppDhcpTrap, BviAclMatchesOnlyDhcpClientBroadcasts)
{
    createTrap(SAI_PACKET_ACTION_COPY);
    addMembers();
    createBvi();

    EXPECT_EQ(bviPunt(), onBvi(ip4Udp(0xff, 67)));
    EXPECT_EQ(MISS, onBvi(ip4Udp(0xff, 68)));       // server to client
    EXPECT_EQ(MISS, onBvi(ip4Udp(0xff, 137)));      // other broadcast UDP
    EXPECT_EQ(MISS, onBvi(ip4Udp(0x01, 67)));       // unicast to 255.255.255.1
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

TEST_F(SwitchVppDhcpTrap, DeletingTheBviUnbindsTheAclFirst)
{
    createTrap(SAI_PACKET_ACTION_COPY);
    addMembers();
    createBvi();
    ASSERT_EQ(1u, g_ip4InputAcl.count(BVI));

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->vpp_delete_bvi_interface(m_rif));

    EXPECT_EQ(0u, g_ip4InputAcl.count(BVI));
    EXPECT_EQ(std::vector<std::string>({"create bvi100", "delete bvi100"}), g_bviCalls);
}
