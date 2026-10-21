#include "vpp/SwitchVpp.h"

#include "meta/sai_serialize.h"

#include <gtest/gtest.h>

#include <algorithm>
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
        std::string name;   // interface, or "<interface> <MAC>" for an L2FIB entry
        uint32_t id;        // VLAN or bridge domain
        uint32_t sub_id;    // return value, or is_static_mac for an L2FIB entry
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

    std::string macStr(
            const uint8_t *mac)
    {
        SWSS_LOG_ENTER();

        char buf[18];

        snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

        return buf;
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

int __wrap_l2fib_add_del(const char *hwif_name, const uint8_t *mac, uint32_t bd_id, bool is_add, bool is_static_mac)
{
    SWSS_LOG_ENTER();

    g_vppCalls.push_back({"l2fib_add_del", std::string(hwif_name ? hwif_name : "") + " " + macStr(mac), bd_id, is_static_mac, is_add});
    return 0;
}

int __wrap_l2fib_flush_all()
{
    SWSS_LOG_ENTER();

    g_vppCalls.push_back({"l2fib_flush_all", "", 0, 0, false});
    return 0;
}

int __wrap_l2fib_flush_int(const char *hwif_name)
{
    SWSS_LOG_ENTER();

    g_vppCalls.push_back({"l2fib_flush_int", hwif_name ? hwif_name : "", 0, 0, false});
    return 0;
}

int __wrap_l2fib_flush_bd(uint32_t bd_id)
{
    SWSS_LOG_ENTER();

    g_vppCalls.push_back({"l2fib_flush_bd", "", bd_id, 0, false});
    return 0;
}

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

            g_vppCalls.clear();
        }

        void TearDown() override
        {
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

TEST_F(SwitchVppLagFdb, MacLearnedOnTheBondIsReportedOnTheLagBridgePort)
{
    vppMacEvent(VPP_MAC_ACTION_ADD);

    auto bp = fdbBridgePort();

    ASSERT_NE(nullptr, bp);
    EXPECT_EQ(m_bridgePort, bp->getAttr()->value.oid);
}

/*
 * orchagent flushes a LAG by its bridge port when the LAG goes operationally
 * down, or when a MAC is learned on it while it is still down. The flush has to
 * reach VPP: otherwise the bond keeps the MAC in its L2FIB (bridge domains do
 * not age), VPP never reports it again, and SONiC loses the MAC for good while
 * VPP keeps forwarding to it.
 */
TEST_F(SwitchVppLagFdb, FlushOfTheLagBridgePortFlushesTheBondAndTheMacIsLearnedAgain)
{
    vppMacEvent(VPP_MAC_ACTION_ADD);

    ASSERT_NE(nullptr, fdbBridgePort());

    EXPECT_EQ(SAI_STATUS_SUCCESS, flushBridgePort());

    auto flushes = vppCallsTo("l2fib_flush_int");

    ASSERT_EQ(1u, flushes.size());
    EXPECT_EQ(BOND_HWIF, flushes[0].name);
    EXPECT_TRUE(vppCallsTo("l2fib_flush_all").empty());

    // the next frame from the host: VPP learns the MAC again and reports it
    vppMacEvent(VPP_MAC_ACTION_ADD);

    auto bp = fdbBridgePort();

    ASSERT_NE(nullptr, bp);
    EXPECT_EQ(m_bridgePort, bp->getAttr()->value.oid);
}

/*
 * m_vpp_fdb_entries mirrors what VPP's L2FIB holds. When VPP deletes a MAC the
 * mirror must forget it even if SAI no longer tracks the MAC (a flush, or any
 * other path that dropped it on the SAI side first). A stale mirror entry makes
 * every later ADD for that MAC look like a duplicate, so it is never reported.
 */
TEST_F(SwitchVppLagFdb, MacDeletedByVppAfterSaiDroppedItIsLearnedAgain)
{
    vppMacEvent(VPP_MAC_ACTION_ADD);

    ASSERT_NE(nullptr, fdbBridgePort());

    // SAI drops the entry without VPP being told
    m_sw->m_objectHash.at(SAI_OBJECT_TYPE_FDB_ENTRY).clear();
    m_sw->m_fdb_info_set.clear();

    vppMacEvent(VPP_MAC_ACTION_DELETE);
    vppMacEvent(VPP_MAC_ACTION_ADD);

    auto bp = fdbBridgePort();

    ASSERT_NE(nullptr, bp);
    EXPECT_EQ(m_bridgePort, bp->getAttr()->value.oid);
}

TEST_F(SwitchVppLagFdb, StaticEntryOnTheLagBridgePortIsProgrammedOnTheBond)
{
    sai_attribute_t attrs[2];

    attrs[0].id = SAI_FDB_ENTRY_ATTR_TYPE;
    attrs[0].value.s32 = SAI_FDB_ENTRY_TYPE_STATIC;
    attrs[1].id = SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID;
    attrs[1].value.oid = m_bridgePort;

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->FdbEntryadd(fdbSid(), m_switchId, 2, attrs));

    auto adds = vppCallsTo("l2fib_add_del");

    ASSERT_EQ(1u, adds.size());
    EXPECT_EQ(std::string(BOND_HWIF) + " 00:50:56:ac:b2:02", adds[0].name);
    EXPECT_EQ(VLAN_ID, adds[0].id);
    EXPECT_TRUE(adds[0].flag);      // is_add
    EXPECT_EQ(1u, adds[0].sub_id);  // is_static_mac

    g_vppCalls.clear();

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->FdbEntrydel(fdbSid()));

    auto dels = vppCallsTo("l2fib_add_del");

    ASSERT_EQ(1u, dels.size());
    EXPECT_EQ(std::string(BOND_HWIF) + " 00:50:56:ac:b2:02", dels[0].name);
    EXPECT_EQ(VLAN_ID, dels[0].id);
    EXPECT_FALSE(dels[0].flag);
}

/*
 * orchagent removing a learned MAC on the LAG (e.g. a MAC that moved behind a
 * VXLAN tunnel) must clear the mirror, or VPP's next learn of it is dropped as
 * a duplicate.
 */
TEST_F(SwitchVppLagFdb, LearnedEntryRemovedOnTheLagBridgePortIsLearnedAgain)
{
    vppMacEvent(VPP_MAC_ACTION_ADD);

    ASSERT_NE(nullptr, fdbBridgePort());

    ASSERT_EQ(SAI_STATUS_SUCCESS, m_sw->FdbEntrydel(fdbSid()));

    ASSERT_EQ(nullptr, fdbBridgePort());

    vppMacEvent(VPP_MAC_ACTION_ADD);

    EXPECT_NE(nullptr, fdbBridgePort());
}

/*
 * VPP's L2FIB flushes are lazy: they bump a sequence number and leave the
 * entry until the next scan, and a frame from the MAC before that scan
 * refreshes the entry without a learn event. A MAC with steady traffic, like a
 * node's, then survives every flush and is never reported again. So a flush has
 * to delete each MAC we know VPP holds in its scope, on the interface VPP
 * reported it on, before the lazy flush that catches the rest.
 */
class SwitchVppFdbFlushDeletes : public SwitchVppLagFdb
{
    protected:

        // a second MAC VPP holds on the bond, in another bridge domain
        void trackOtherMac()
        {
            SWSS_LOG_ENTER();

            VppFdbKeyAlias key;

            memcpy(key.mac, OTHER_MAC, sizeof(key.mac));
            key.bd_id = OTHER_BD;

            m_sw->m_vpp_fdb_entries[key] = BOND_SW_IF_INDEX;
        }

        // position of the first call to api in g_vppCalls, or -1
        static int callIndex(
                const std::string& api,
                const std::string& name = "")
        {
            SWSS_LOG_ENTER();

            for (size_t i = 0; i < g_vppCalls.size(); i++)
            {
                if (g_vppCalls[i].api == api && (name.empty() || g_vppCalls[i].name == name))
                {
                    return (int)i;
                }
            }

            return -1;
        }

        sai_status_t flushAttrs(
                std::vector<sai_attribute_t> attrs)
        {
            SWSS_LOG_ENTER();

            return m_sw->vpp_fdbentry_flush(m_switchId, (uint32_t)attrs.size(), attrs.data());
        }

        static sai_attribute_t dynamicOnly()
        {
            SWSS_LOG_ENTER();

            sai_attribute_t attr;

            memset(&attr, 0, sizeof(attr));
            attr.id = SAI_FDB_FLUSH_ATTR_ENTRY_TYPE;
            attr.value.s32 = SAI_FDB_FLUSH_ENTRY_TYPE_DYNAMIC;

            return attr;
        }

        using VppFdbKeyAlias = SwitchVpp::VppFdbKey;

        static constexpr uint32_t OTHER_BD = 906;
        static constexpr uint8_t OTHER_MAC[6] = { 0x00, 0x50, 0x56, 0xac, 0xb2, 0x03 };

        const std::string MAC_ON_BOND = std::string(BOND_HWIF) + " 00:50:56:ac:b2:02";
        const std::string OTHER_MAC_ON_BOND = std::string(BOND_HWIF) + " 00:50:56:ac:b2:03";
};

constexpr uint32_t SwitchVppFdbFlushDeletes::OTHER_BD;
constexpr uint8_t SwitchVppFdbFlushDeletes::OTHER_MAC[6];

TEST_F(SwitchVppFdbFlushDeletes, FlushOfTheLagBridgePortDeletesItsMacBeforeTheLazyFlush)
{
    vppMacEvent(VPP_MAC_ACTION_ADD);

    ASSERT_NE(nullptr, fdbBridgePort());

    // a MAC on an interface that is not this bridge port's stays in VPP
    VppFdbKeyAlias elsewhere;
    memcpy(elsewhere.mac, OTHER_MAC, sizeof(elsewhere.mac));
    elsewhere.bd_id = VLAN_ID;
    m_sw->m_vpp_fdb_entries[elsewhere] = 999;

    g_vppCalls.clear();

    EXPECT_EQ(SAI_STATUS_SUCCESS, flushBridgePort());

    auto dels = vppCallsTo("l2fib_add_del");

    ASSERT_EQ(1u, dels.size());
    EXPECT_EQ(MAC_ON_BOND, dels[0].name);
    EXPECT_EQ(VLAN_ID, dels[0].id);
    EXPECT_FALSE(dels[0].flag);         // is_add
    EXPECT_EQ(0u, dels[0].sub_id);      // is_static_mac

    int del = callIndex("l2fib_add_del", MAC_ON_BOND);
    int flush = callIndex("l2fib_flush_int", BOND_HWIF);

    ASSERT_NE(-1, flush);
    EXPECT_LT(del, flush);

    // forgotten as well, so VPP's next learn of it is reported
    EXPECT_EQ(1u, m_sw->m_vpp_fdb_entries.size());
    EXPECT_EQ(1u, m_sw->m_vpp_fdb_entries.count(elsewhere));
}

TEST_F(SwitchVppFdbFlushDeletes, FlushOfABridgeDomainDeletesOnlyItsMacs)
{
    vppMacEvent(VPP_MAC_ACTION_ADD);

    ASSERT_NE(nullptr, fdbBridgePort());

    trackOtherMac();

    g_vppCalls.clear();

    sai_attribute_t bv;

    memset(&bv, 0, sizeof(bv));
    bv.id = SAI_FDB_FLUSH_ATTR_BV_ID;
    bv.value.u16 = VLAN_ID;     // what vpp_fdbentry_flush() reads the BV_ID as

    EXPECT_EQ(SAI_STATUS_SUCCESS, flushAttrs({bv, dynamicOnly()}));

    auto dels = vppCallsTo("l2fib_add_del");

    ASSERT_EQ(1u, dels.size());
    EXPECT_EQ(MAC_ON_BOND, dels[0].name);
    EXPECT_EQ(VLAN_ID, dels[0].id);
    EXPECT_FALSE(dels[0].flag);

    auto flushes = vppCallsTo("l2fib_flush_bd");

    ASSERT_EQ(1u, flushes.size());
    EXPECT_EQ(VLAN_ID, flushes[0].id);
    EXPECT_LT(callIndex("l2fib_add_del"), callIndex("l2fib_flush_bd"));

    EXPECT_EQ(1u, m_sw->m_vpp_fdb_entries.size());
}

TEST_F(SwitchVppFdbFlushDeletes, FlushAllDeletesEveryTrackedMac)
{
    vppMacEvent(VPP_MAC_ACTION_ADD);

    ASSERT_NE(nullptr, fdbBridgePort());

    trackOtherMac();

    g_vppCalls.clear();

    EXPECT_EQ(SAI_STATUS_SUCCESS, flushAttrs({dynamicOnly()}));

    auto dels = vppCallsTo("l2fib_add_del");

    ASSERT_EQ(2u, dels.size());

    std::vector<std::pair<std::string, uint32_t>> deleted;

    for (auto& d: dels)
    {
        EXPECT_FALSE(d.flag);
        deleted.push_back({d.name, d.id});
    }

    std::sort(deleted.begin(), deleted.end());

    EXPECT_EQ(MAC_ON_BOND, deleted[0].first);
    EXPECT_EQ((uint32_t)VLAN_ID, deleted[0].second);
    EXPECT_EQ(OTHER_MAC_ON_BOND, deleted[1].first);
    EXPECT_EQ(OTHER_BD, deleted[1].second);

    int flush = callIndex("l2fib_flush_all");

    ASSERT_NE(-1, flush);
    EXPECT_LT(callIndex("l2fib_add_del"), flush);

    EXPECT_TRUE(m_sw->m_vpp_fdb_entries.empty());
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
