#include "vpp/SwitchVpp.h"

#include "meta/sai_serialize.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
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
        std::string name;   // interface, and the MAC for an L2FIB entry
        uint32_t id;        // bridge domain
        uint32_t sub_id;    // is_static_mac
        bool flag;          // is_add
    };

    std::vector<VppCall> g_vppCalls;

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
