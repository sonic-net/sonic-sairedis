#include "MockVppXlate.h"
#include "meta/sai_serialize.h"
#include "saivs.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <arpa/inet.h>
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

        sai_object_id_t createObject(
                sai_object_type_t type,
                std::vector<sai_attribute_t> attrs)
        {
            SWSS_LOG_ENTER();

            auto oid = m_mgr->allocateNewObjectId(type, m_switchId);

            check(m_sw->create(type, sai_serialize_object_id(oid), m_switchId,
                        (uint32_t)attrs.size(), attrs.data()) == SAI_STATUS_SUCCESS,
                    sai_serialize_object_type(type).c_str());

            return oid;
        }

        void removeObject(
                sai_object_type_t type,
                sai_object_id_t oid)
        {
            SWSS_LOG_ENTER();

            check(m_sw->remove(type, sai_serialize_object_id(oid)) == SAI_STATUS_SUCCESS,
                    sai_serialize_object_type(type).c_str());
        }

        /*
         * The SAI calls orchagent makes for an ingress ACL on a port: the
         * port's ACL table group is bound when the port is created, a table
         * is bound to the port by adding it to that group, and rules come and
         * go in the table. ip2me wants its feature on a port while a table
         * bound there has a deny rule.
         */
        void bindIngressAclGroup()
        {
            SWSS_LOG_ENTER();

            sai_attribute_t attr = {};

            attr.id = SAI_ACL_TABLE_GROUP_ATTR_ACL_STAGE;
            attr.value.s32 = SAI_ACL_STAGE_INGRESS;

            m_aclGroup = createObject(SAI_OBJECT_TYPE_ACL_TABLE_GROUP, { attr });

            attr.id = SAI_PORT_ATTR_INGRESS_ACL;
            attr.value.oid = m_aclGroup;

            check(m_sw->set(SAI_OBJECT_TYPE_PORT, sai_serialize_object_id(m_port), &attr) == SAI_STATUS_SUCCESS,
                    "port bound to its ingress ACL group");
        }

        sai_object_id_t createAclTable()
        {
            SWSS_LOG_ENTER();

            std::vector<sai_attribute_t> attrs(2);

            attrs[0].id = SAI_ACL_TABLE_ATTR_ACL_STAGE;
            attrs[0].value.s32 = SAI_ACL_STAGE_INGRESS;
            attrs[1].id = SAI_ACL_TABLE_ATTR_FIELD_SRC_IP;
            attrs[1].value.booldata = true;

            return createObject(SAI_OBJECT_TYPE_ACL_TABLE, attrs);
        }

        sai_object_id_t bindAclTable(
                sai_object_id_t table)
        {
            SWSS_LOG_ENTER();

            std::vector<sai_attribute_t> attrs(3);

            attrs[0].id = SAI_ACL_TABLE_GROUP_MEMBER_ATTR_ACL_TABLE_GROUP_ID;
            attrs[0].value.oid = m_aclGroup;
            attrs[1].id = SAI_ACL_TABLE_GROUP_MEMBER_ATTR_ACL_TABLE_ID;
            attrs[1].value.oid = table;
            attrs[2].id = SAI_ACL_TABLE_GROUP_MEMBER_ATTR_PRIORITY;
            attrs[2].value.u32 = 100;

            return createObject(SAI_OBJECT_TYPE_ACL_TABLE_GROUP_MEMBER, attrs);
        }

        sai_object_id_t addDenyRule(
                sai_object_id_t table)
        {
            SWSS_LOG_ENTER();

            std::vector<sai_attribute_t> attrs(4);

            attrs[0].id = SAI_ACL_ENTRY_ATTR_TABLE_ID;
            attrs[0].value.oid = table;
            attrs[1].id = SAI_ACL_ENTRY_ATTR_PRIORITY;
            attrs[1].value.u32 = 10;
            attrs[2].id = SAI_ACL_ENTRY_ATTR_FIELD_SRC_IP;
            attrs[2].value.aclfield.enable = true;
            attrs[2].value.aclfield.data.ip4 = htonl(0x0a000001);
            attrs[2].value.aclfield.mask.ip4 = 0xffffffff;
            attrs[3].id = SAI_ACL_ENTRY_ATTR_ACTION_PACKET_ACTION;
            attrs[3].value.aclaction.enable = true;
            attrs[3].value.aclaction.parameter.s32 = SAI_PACKET_ACTION_DROP;

            return createObject(SAI_OBJECT_TYPE_ACL_ENTRY, attrs);
        }

        // next nodes of the classify sessions in a table
        std::set<uint32_t> sessionNexts(
                uint32_t table)
        {
            SWSS_LOG_ENTER();

            std::set<uint32_t> nexts;

            for (auto& call : vppMock().callsTo("vpp_classify_session_add", ""))
            {
                if (call.id == table)
                {
                    nexts.insert(call.sub_id);
                }
            }

            return nexts;
        }

        // enable flag of each ip2me call made for the port, in order
        std::vector<bool> ip2meCalls()
        {
            SWSS_LOG_ENTER();

            std::vector<bool> enables;

            for (auto& call : vppMock().callsTo("vpp_sonic_ext_ip2me_enable_disable", PORT_HWIF))
            {
                enables.push_back(call.flag);
            }

            return enables;
        }

        static constexpr uint16_t VLAN_ID = 100;
        static constexpr const char *PORT_HWIF = "bobm1";
        static constexpr const char *OTHER_PORT_HWIF = "bobm10";
        static constexpr const char *TAGGED_MEMBER_HWIF = "bobm1.100";
        static constexpr uint32_t NO_TABLE = ~0u;

        const sai_object_id_t m_switchId = 0x2100000000;

        std::shared_ptr<RealObjectIdManager> m_mgr;

        std::shared_ptr<SwitchVpp> m_sw;

        sai_object_id_t m_vlan = SAI_NULL_OBJECT_ID;
        sai_object_id_t m_port = SAI_NULL_OBJECT_ID;
        sai_object_id_t m_bridgePort = SAI_NULL_OBJECT_ID;
        sai_object_id_t m_otherPort = SAI_NULL_OBJECT_ID;
        sai_object_id_t m_otherBridgePort = SAI_NULL_OBJECT_ID;
        sai_object_id_t m_aclGroup = SAI_NULL_OBJECT_ID;
};

constexpr uint16_t SwitchVppSonicExtFeature::VLAN_ID;
constexpr const char *SwitchVppSonicExtFeature::PORT_HWIF;
constexpr const char *SwitchVppSonicExtFeature::OTHER_PORT_HWIF;
constexpr const char *SwitchVppSonicExtFeature::TAGGED_MEMBER_HWIF;
constexpr uint32_t SwitchVppSonicExtFeature::NO_TABLE;

TEST_F(SwitchVppSonicExtFeature, ip2meFollowsAclLifecycleWhenEnabled)
{
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
    ASSERT_EXIT({
        vppMock().enabledFeatures.insert("ip2me");

        bindIngressAclGroup();
        auto table = createAclTable();
        auto member = bindAclTable(table);

        check(ip2meCalls().empty(), "ip2me not programmed without a deny rule");

        auto rule = addDenyRule(table);
        check(ip2meCalls() == std::vector<bool>({true}), "adding a deny rule enables ip2me");

        removeObject(SAI_OBJECT_TYPE_ACL_ENTRY, rule);
        check(ip2meCalls() == std::vector<bool>({true, false}), "removing the deny rule disables ip2me");

        addDenyRule(table);
        check(ip2meCalls() == std::vector<bool>({true, false, true}), "adding it again enables ip2me again");

        removeObject(SAI_OBJECT_TYPE_ACL_TABLE_GROUP_MEMBER, member);
        check(ip2meCalls() == std::vector<bool>({true, false, true, false}), "unbinding the table disables ip2me");

        bindAclTable(table);
        check(ip2meCalls() == std::vector<bool>({true, false, true, false, true}),
                "binding it again enables ip2me again");
        check(m_sw->m_ip2me_enabled_ports.count(PORT_HWIF) == 1, "port tracked as ip2me enabled");
        _exit(0);
    }, ::testing::ExitedWithCode(0), "");
}

TEST_F(SwitchVppSonicExtFeature, ip2meDisabledAtStartupIsNeverEnabled)
{
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
    ASSERT_EXIT({
        bindIngressAclGroup();
        auto table = createAclTable();
        auto member = bindAclTable(table);
        auto rule = addDenyRule(table);
        removeObject(SAI_OBJECT_TYPE_ACL_ENTRY, rule);
        addDenyRule(table);
        removeObject(SAI_OBJECT_TYPE_ACL_TABLE_GROUP_MEMBER, member);
        bindAclTable(table);

        check(ip2meCalls().empty(), "ip2me never programmed");
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

        bindIngressAclGroup();
        auto table = createAclTable();
        auto member = bindAclTable(table);
        addDenyRule(table);

        check(ip2meCalls().empty(), "ip2me not programmed while its feature query fails");

        // a failed answer is not kept, so the next ACL change on the port asks again
        vppMock().featureResult = 0;
        removeObject(SAI_OBJECT_TYPE_ACL_TABLE_GROUP_MEMBER, member);
        bindAclTable(table);

        check(ip2meCalls() == std::vector<bool>({true}), "ip2me enabled once the query succeeds");
        _exit(0);
    }, ::testing::ExitedWithCode(0), "");
}

TEST_F(SwitchVppSonicExtFeature, ip2meFailedProgrammingIsRetried)
{
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
    ASSERT_EXIT({
        vppMock().enabledFeatures.insert("ip2me");
        vppMock().ip2meResult = -1;

        bindIngressAclGroup();
        auto table = createAclTable();
        auto member = bindAclTable(table);
        addDenyRule(table);

        check(ip2meCalls() == std::vector<bool>({true}), "ip2me enable attempted");
        check(m_sw->m_ip2me_enabled_ports.empty(), "a failed enable is not tracked as enabled");

        vppMock().ip2meResult = 0;
        removeObject(SAI_OBJECT_TYPE_ACL_TABLE_GROUP_MEMBER, member);
        bindAclTable(table);

        check(ip2meCalls() == std::vector<bool>({true, true}), "binding the table again retries the enable");
        check(m_sw->m_ip2me_enabled_ports.count(PORT_HWIF) == 1, "port tracked once enabled");
        _exit(0);
    }, ::testing::ExitedWithCode(0), "");
}

/*
 * The first VLAN member sets up the l2-input-classify punt tables. Their next
 * nodes are the Linux control plane punt node, always, and the sonic-ext
 * l2-trap-fixup and l2-vlan-filter nodes, each only while its feature is on.
 *
 * Without flood control on the VLAN, an untagged member gets the DHCP table
 * in its IP4 slot and the LLDP table, which also filters tagged frames, in
 * its other slot. A tagged member only gets the tagged DHCP table, which
 * exists only with l2-trap-fixup.
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

                for (auto& call : vppMock().callsTo("vpp_classify_session_add", ""))
                {
                    check(nexts.count(call.sub_id) == 1, "every session hits an added next node");
                }

                auto untagged = vppMock().callsTo("vpp_classify_set_interface_l2_tables", OTHER_PORT_HWIF);
                auto tagged = vppMock().callsTo("vpp_classify_set_interface_l2_tables", TAGGED_MEMBER_HWIF);

                check(untagged.size() == 1 && untagged[0].flag, "untagged member's input tables set once");
                check(tagged.size() == 1 && tagged[0].flag, "tagged member's input tables set once");

                std::set<uint32_t> lldp = {punt[0].id};
                if (vlanFilter)
                {
                    lldp.insert(filter[0].id);
                }

                check(untagged[0].sub_id != NO_TABLE &&
                        sessionNexts(untagged[0].sub_id) == std::set<uint32_t>({punt[0].id}),
                        "untagged DHCP table punts to linux-cp-punt");
                check(untagged[0].id != NO_TABLE && sessionNexts(untagged[0].id) == lldp,
                        "untagged LLDP table punts, and filters tagged frames only with l2-vlan-filter");

                check(tagged[0].sub_id == NO_TABLE, "tagged member gets no IP4 table");
                if (trapFixup)
                {
                    check(tagged[0].id != NO_TABLE &&
                            sessionNexts(tagged[0].id) == std::set<uint32_t>({fixup[0].id}),
                            "tagged DHCP table goes through l2-trap-fixup");
                }
                else
                {
                    check(tagged[0].id == NO_TABLE, "tagged member gets no punt table without l2-trap-fixup");
                }
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
