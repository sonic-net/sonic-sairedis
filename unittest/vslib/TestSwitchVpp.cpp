#include "vpp/SwitchVpp.h"

#include "meta/sai_serialize.h"

#include <gtest/gtest.h>

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
    /*
     * VPP sends L2_MACS_EVENT on the command socket. These are the events it
     * has sent that nothing has read yet; vpp_l2_macs_events_poll() hands them
     * to the callback the switch registered with vpp_want_l2_macs_events2().
     * No other stub reads them, so the tests see a SONiC that makes no
     * unrelated VPP call.
     */
    std::vector<vpp_mac_event_t> g_unreadMacEvents;
    vpp_mac_event_cb_fn g_macEventCb = nullptr;
    void *g_macEventCtx = nullptr;
}

extern "C" {

int __wrap_init_vpp_client() { return 0; }
int __wrap_vpp_sync_for_events() { return 0; }
vpp_event_info_t * __wrap_vpp_ev_dequeue() { return NULL; }
int __wrap_vpp_want_l2_macs_events2(bool enable, vpp_mac_event_cb_fn cb, void *ctx)
{
    SWSS_LOG_ENTER();

    g_macEventCb = enable ? cb : nullptr;
    g_macEventCtx = enable ? ctx : nullptr;
    return 0;
}

int __wrap_vpp_l2fib_set_scan_delay(uint16_t delay_10ms) { return 0; }

int __wrap_vpp_l2_macs_events_poll()
{
    SWSS_LOG_ENTER();

    if (g_unreadMacEvents.empty())
    {
        return 0;
    }

    std::vector<vpp_mac_event_t> evs;

    evs.swap(g_unreadMacEvents);

    if (g_macEventCb)
    {
        g_macEventCb(evs.data(), (uint32_t)evs.size(), g_macEventCtx);
    }

    return 1;
}
int __wrap_refresh_interfaces_list() { return 0; }

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
        }

        void TearDown() override
        {
            m_sw.reset();
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
 * VPP sends its MAC events on the command socket, which is otherwise read
 * only while a call waits for its reply. A MAC VPP learns while SONiC makes no
 * other VPP call, like a host that sends a frame now and then, or comes back
 * after its PortChannel flapped, must still be reported: the FDB aging pass
 * reads the pending events itself instead of waiting for an unrelated call.
 */
class SwitchVppIdleMacEvents : public SwitchVppLagFdb
{
    protected:

        void SetUp() override
        {
            SwitchVppLagFdb::SetUp();

            g_unreadMacEvents.clear();

            // what Sai::startFdbAgingThread() does
            m_sw->initFdbEventHandling([]() {});

            ASSERT_NE(nullptr, g_macEventCb);
        }

        void TearDown() override
        {
            g_unreadMacEvents.clear();

            SwitchVppLagFdb::TearDown();
        }

        // VPP learns MAC on the bond and sends the event; nothing reads it yet
        void vppSendsMacEvent(
                uint8_t action)
        {
            SWSS_LOG_ENTER();

            vpp_mac_event_t ev;

            memcpy(ev.mac, MAC, sizeof(ev.mac));
            ev.sw_if_index = BOND_SW_IF_INDEX;
            ev.action = action;

            g_unreadMacEvents.push_back(ev);
        }
};

TEST_F(SwitchVppIdleMacEvents, MacLearnedWhileSonicIsIdleIsReportedByTheAgingPass)
{
    vppSendsMacEvent(VPP_MAC_ACTION_ADD);

    // the FDB aging thread's 1 s tick; no other VPP call happens
    m_sw->processFdbEntriesForAging();

    auto bp = fdbBridgePort();

    ASSERT_NE(nullptr, bp);
    EXPECT_EQ(m_bridgePort, bp->getAttr()->value.oid);
    EXPECT_TRUE(g_unreadMacEvents.empty());
}

/*
 * The DELETE events wait in the same socket. A MAC VPP removes from its L2FIB
 * and then learns again while SONiC is idle, like a quiet host behind a
 * PortChannel that flapped, must be aged and then reported again.
 */
TEST_F(SwitchVppIdleMacEvents, MacRemovedAndLearnedAgainWhileSonicIsIdleIsReported)
{
    vppSendsMacEvent(VPP_MAC_ACTION_ADD);
    m_sw->processFdbEntriesForAging();

    ASSERT_NE(nullptr, fdbBridgePort());

    vppSendsMacEvent(VPP_MAC_ACTION_DELETE);
    m_sw->processFdbEntriesForAging();

    ASSERT_EQ(nullptr, fdbBridgePort());

    vppSendsMacEvent(VPP_MAC_ACTION_ADD);
    m_sw->processFdbEntriesForAging();

    auto bp = fdbBridgePort();

    ASSERT_NE(nullptr, bp);
    EXPECT_EQ(m_bridgePort, bp->getAttr()->value.oid);
}
