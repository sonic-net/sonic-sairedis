#include "MockVppXlate.h"
#include "meta/sai_serialize.h"
#include "saivs.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdio>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <unistd.h>

using namespace saivs;

/*
 * The sonic-ext features saivpp wires itself (ip2me, l2-trap-fixup,
 * l2-vlan-filter) are switched off in VPP's startup config, and saivpp learns
 * that over sonic_ext_feature_get. Its answers are cached for the life of the
 * process, and the l2 classify punt setup runs once per process too, so each
 * case runs in a child process of its own.
 */
namespace
{
    // Called in the child: a failed check ends it, and the parent prints why.
    void check(
            bool condition,
            const char *what)
    {
        SWSS_LOG_ENTER();

        if (!condition)
        {
            fprintf(stderr, "check failed: %s\n", what);
            _exit(1);
        }
    }

    bool queried(
            const std::string& feature)
    {
        SWSS_LOG_ENTER();

        const auto& q = vppMock().queriedFeatures;

        return std::find(q.begin(), q.end(), feature) != q.end();
    }
}

/*
 * Two front panel ports behind bridge ports, and VLAN 100, as in
 * SwitchVppSubIfAdminState.
 */
class SwitchVppSonicExtFeature : public ::testing::Test
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

        sai_status_t addMember(
                sai_object_id_t bridgePort,
                sai_vlan_tagging_mode_t mode)
        {
            SWSS_LOG_ENTER();

            auto member = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_VLAN_MEMBER, m_switchId);

            sai_attribute_t attrs[3];

            attrs[0].id = SAI_VLAN_MEMBER_ATTR_VLAN_ID;
            attrs[0].value.oid = m_vlan;
            attrs[1].id = SAI_VLAN_MEMBER_ATTR_BRIDGE_PORT_ID;
            attrs[1].value.oid = bridgePort;
            attrs[2].id = SAI_VLAN_MEMBER_ATTR_VLAN_TAGGING_MODE;
            attrs[2].value.s32 = mode;

            return m_sw->createVlanMember(member, m_switchId, 3, attrs);
        }

        // ip2me wants its feature on a port that has an ingress table with a deny rule bound
        void bindDropTable(
                sai_object_id_t table)
        {
            SWSS_LOG_ENTER();

            m_sw->m_port_acl_tables[PORT_HWIF].ingress.insert(table);
            m_sw->ip2meUpdateDropTable(table, true);
        }

        static constexpr uint16_t VLAN_ID = 100;
        static constexpr const char *PORT_HWIF = "bobm1";
        static constexpr const char *OTHER_PORT_HWIF = "bobm10";

        const sai_object_id_t m_switchId = 0x2100000000;

        std::shared_ptr<RealObjectIdManager> m_mgr;

        std::shared_ptr<SwitchVpp> m_sw;

        sai_object_id_t m_vlan = SAI_NULL_OBJECT_ID;
        sai_object_id_t m_port = SAI_NULL_OBJECT_ID;
        sai_object_id_t m_bridgePort = SAI_NULL_OBJECT_ID;
        sai_object_id_t m_otherPort = SAI_NULL_OBJECT_ID;
        sai_object_id_t m_otherBridgePort = SAI_NULL_OBJECT_ID;
};

constexpr uint16_t SwitchVppSonicExtFeature::VLAN_ID;
constexpr const char *SwitchVppSonicExtFeature::PORT_HWIF;
constexpr const char *SwitchVppSonicExtFeature::OTHER_PORT_HWIF;

TEST_F(SwitchVppSonicExtFeature, ip2meFollowsDropTableWhenEnabled)
{
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
    ASSERT_EXIT({
        vppMock().enabledFeatures.insert("ip2me");

        auto table = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_ACL_TABLE, m_switchId);

        bindDropTable(table);

        auto calls = vppMock().callsTo("vpp_sonic_ext_ip2me_enable_disable", PORT_HWIF);
        check(calls.size() == 1 && calls[0].flag, "ip2me enabled on the port");

        m_sw->ip2meUpdateDropTable(table, false);

        calls = vppMock().callsTo("vpp_sonic_ext_ip2me_enable_disable", PORT_HWIF);
        check(calls.size() == 2 && !calls[1].flag, "ip2me disabled once the deny rule is gone");
        check(queried("ip2me"), "ip2me queried");
        _exit(0);
    }, ::testing::ExitedWithCode(0), "");
}

TEST_F(SwitchVppSonicExtFeature, ip2meDisabledAtStartupIsNeverEnabled)
{
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
    ASSERT_EXIT({
        auto table = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_ACL_TABLE, m_switchId);

        bindDropTable(table);
        m_sw->ip2meUpdateDropTable(table, false);
        bindDropTable(table);

        check(vppMock().callsTo("vpp_sonic_ext_ip2me_enable_disable", PORT_HWIF).empty(),
                "ip2me never programmed");
        check(m_sw->m_ip2me_enabled_ports.empty(), "no port tracked as ip2me enabled");
        check(queried("ip2me"), "ip2me queried");
        _exit(0);
    }, ::testing::ExitedWithCode(0), "");
}

TEST_F(SwitchVppSonicExtFeature, ip2meFailedFeatureQueryIsNotEnabled)
{
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
    ASSERT_EXIT({
        vppMock().enabledFeatures.insert("ip2me");
        vppMock().featureResult = -1;

        auto table = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_ACL_TABLE, m_switchId);

        bindDropTable(table);

        check(vppMock().callsTo("vpp_sonic_ext_ip2me_enable_disable", PORT_HWIF).empty(),
                "ip2me not programmed while its feature query fails");

        // a failed answer is not kept, so the next refresh asks again
        vppMock().featureResult = 0;
        m_sw->ip2meRefreshPort(PORT_HWIF);

        auto calls = vppMock().callsTo("vpp_sonic_ext_ip2me_enable_disable", PORT_HWIF);
        check(calls.size() == 1 && calls[0].flag, "ip2me enabled once the query succeeds");
        _exit(0);
    }, ::testing::ExitedWithCode(0), "");
}

TEST_F(SwitchVppSonicExtFeature, ip2meFailedProgrammingIsRetried)
{
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
    ASSERT_EXIT({
        vppMock().enabledFeatures.insert("ip2me");
        vppMock().ip2meResult = -1;

        auto table = m_mgr->allocateNewObjectId(SAI_OBJECT_TYPE_ACL_TABLE, m_switchId);

        bindDropTable(table);

        check(vppMock().callsTo("vpp_sonic_ext_ip2me_enable_disable", PORT_HWIF).size() == 1,
                "ip2me enable attempted");
        check(m_sw->m_ip2me_enabled_ports.empty(), "a failed enable is not tracked as enabled");

        vppMock().ip2meResult = 0;
        m_sw->ip2meRefreshPort(PORT_HWIF);

        auto calls = vppMock().callsTo("vpp_sonic_ext_ip2me_enable_disable", PORT_HWIF);
        check(calls.size() == 2 && calls[1].flag, "the next refresh retries the enable");
        check(m_sw->m_ip2me_enabled_ports.count(PORT_HWIF) == 1, "port tracked once enabled");
        _exit(0);
    }, ::testing::ExitedWithCode(0), "");
}

/*
 * The first VLAN member sets up the l2-input-classify punt tables. Their next
 * nodes are the Linux control plane punt node, always, and the sonic-ext
 * l2-trap-fixup and l2-vlan-filter nodes, each only while its feature is on.
 */
TEST_F(SwitchVppSonicExtFeature, l2ClassifyNextNodesFollowFeatures)
{
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
    for (bool trapFixup : {true, false})
    {
        for (bool vlanFilter : {true, false})
        {
            SCOPED_TRACE(std::string("l2-trap-fixup ") + (trapFixup ? "on" : "off") +
                    ", l2-vlan-filter " + (vlanFilter ? "on" : "off"));
            ASSERT_EXIT({
                vppMock().addNodeNextResult = 0;
                if (trapFixup)
                {
                    vppMock().enabledFeatures.insert("l2-trap-fixup");
                }
                if (vlanFilter)
                {
                    vppMock().enabledFeatures.insert("l2-vlan-filter");
                }

                check(addMember(m_bridgePort, SAI_VLAN_TAGGING_MODE_TAGGED) == SAI_STATUS_SUCCESS,
                        "tagged member created");
                check(addMember(m_otherBridgePort, SAI_VLAN_TAGGING_MODE_UNTAGGED) == SAI_STATUS_SUCCESS,
                        "untagged member created");

                auto punt = vppMock().callsTo("vpp_add_node_next", "linux-cp-punt");
                auto fixup = vppMock().callsTo("vpp_add_node_next", "sonic-ext-l2-trap-fixup");
                auto filter = vppMock().callsTo("vpp_add_node_next", "sonic-ext-l2-vlan-filter");

                check(punt.size() == 1, "linux-cp-punt added once");
                check(fixup.size() == (trapFixup ? 1u : 0u), "l2-trap-fixup next follows its feature");
                check(filter.size() == (vlanFilter ? 1u : 0u), "l2-vlan-filter next follows its feature");
                check(queried("l2-trap-fixup") && queried("l2-vlan-filter"), "both features queried");

                std::set<uint32_t> nexts = {punt[0].id};
                for (auto& call : fixup)
                {
                    nexts.insert(call.id);
                }
                for (auto& call : filter)
                {
                    nexts.insert(call.id);
                }

                std::set<uint32_t> hits;
                for (auto& call : vppMock().callsTo("vpp_classify_session_add", ""))
                {
                    check(nexts.count(call.sub_id) == 1, "every session hits an added next node");
                    hits.insert(call.sub_id);
                }

                check(hits.count(punt[0].id) == 1, "a session punts to linux-cp-punt");
                check(!trapFixup || hits.count(fixup[0].id) == 1, "a session uses l2-trap-fixup");
                check(!vlanFilter || hits.count(filter[0].id) == 1, "a session uses l2-vlan-filter");
                _exit(0);
            }, ::testing::ExitedWithCode(0), "");
        }
    }
}

TEST_F(SwitchVppSonicExtFeature, l2FailedFeatureQueryAddsNoFeatureNodes)
{
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
    ASSERT_EXIT({
        vppMock().addNodeNextResult = 0;
        vppMock().enabledFeatures.insert("l2-trap-fixup");
        vppMock().enabledFeatures.insert("l2-vlan-filter");
        vppMock().featureResult = -1;

        check(addMember(m_bridgePort, SAI_VLAN_TAGGING_MODE_TAGGED) == SAI_STATUS_SUCCESS,
                "tagged member created");
        check(addMember(m_otherBridgePort, SAI_VLAN_TAGGING_MODE_UNTAGGED) == SAI_STATUS_SUCCESS,
                "untagged member created");

        auto punt = vppMock().callsTo("vpp_add_node_next", "linux-cp-punt");

        check(punt.size() == 1, "linux-cp-punt still added");
        check(vppMock().callsTo("vpp_add_node_next", "sonic-ext-l2-trap-fixup").empty(),
                "l2-trap-fixup not added while its query fails");
        check(vppMock().callsTo("vpp_add_node_next", "sonic-ext-l2-vlan-filter").empty(),
                "l2-vlan-filter not added while its query fails");

        for (auto& call : vppMock().callsTo("vpp_classify_session_add", ""))
        {
            check(call.sub_id == punt[0].id, "every session punts to linux-cp-punt");
        }
        _exit(0);
    }, ::testing::ExitedWithCode(0), "");
}
