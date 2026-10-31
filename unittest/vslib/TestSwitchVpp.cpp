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
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <unistd.h>

using namespace saivs;

namespace
{
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

    // IPv4 API routes VPP holds: (table, prefix) -> next hop type
    std::map<std::pair<uint32_t, std::string>, int> g_ip4Routes;

    // the IP2ME address set: the destinations the IP2ME policer meters on the punt path
    std::set<std::string> g_ip2meAddrs;
}

extern "C" {

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

int __wrap_ip_route_add_del(vpp_ip_route_t *route, bool is_add)
{
    SWSS_LOG_ENTER();

    EXPECT_EQ(AF_INET, route->prefix_addr.sa_family);
    EXPECT_EQ(1u, route->nexthop_cnt);

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
            g_classifyAvailable = true;
            g_l2Tables.clear();
            g_ip4Routes.clear();
            g_ip2meAddrs.clear();
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

            return nodeNext("l2-input-classify", "linux-cp-punt");
        }

        static uint32_t tagFixup()
        {
            SWSS_LOG_ENTER();

            return nodeNext("l2-input-classify", "sonic-ext-l2-trap-fixup");
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
