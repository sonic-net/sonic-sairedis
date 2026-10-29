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
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <set>
#include <vector>
#include <unistd.h>

using namespace saivs;

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
            {"untagged ARP", {PUNT}},
            {"untagged IPv6 multicast", {PUNT}},
            {"tagged DHCPv4", {TRAP_FIXUP}},
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
        {"untagged ARP", {TRAP_LEARN}},
        {"untagged IPv6 multicast", {TRAP_LEARN}},
        {"tagged DHCPv4", {TRAP_LEARN}},
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

// a sonic-ext plugin that reports l2-trap-learn on without having its node
TEST_F(SwitchVppL2TrapLearn, TrapsKeepTheirNextsWhenVppHasNoTrapLearnNode)
{
    auto nodes = NODES;

    nodes.erase("sonic-ext-l2-trap-learn");

    expectPuntNexts(FEATURES, nodes, NEXTS_WITHOUT_TRAP_LEARN);
}

// sonic-ext-l2-trap-learn sends its copy of the frame to
// sonic-ext-l2-trap-fixup, so it isn't used without it
TEST_F(SwitchVppL2TrapLearn, TrapsDoNotGoToTrapLearnWithoutTrapFixup)
{
    expectPuntNexts({"l2-trap-learn", "l2-vlan-filter"}, NODES, {
        {"untagged DHCPv4", {PUNT}},
        {"untagged ARP", {PUNT}},
        {"untagged IPv6 multicast", {PUNT}},
        {"LLDP", {PUNT}},
        {"VLAN filter", {VLAN_FILTER}},
    });
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
