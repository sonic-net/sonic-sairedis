#include "vpp/SwitchVpp.h"
#include "vpp/vppxlate/SaiIntfStats.h"
#include "ContextConfigContainer.h"
#include "meta/DummySaiInterface.h"
#include "meta/Meta.h"
#include "meta/sai_serialize.h"
#include "saivs.h"

#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <string>
#include <set>
#include <vector>
#include <unistd.h>

using namespace saivs;

namespace
{
    struct VppCall
    {
        std::string api;
        std::string name;
        uint32_t id;
        uint32_t sub_id;
        bool flag;
    };

    std::vector<VppCall> g_vppCalls;
    std::map<std::string, bool> g_vppAdminUp;

    bool vppAdminUp(const std::string& hwif)
    {
        SWSS_LOG_ENTER();
        auto it = g_vppAdminUp.find(hwif);
        return it != g_vppAdminUp.end() && it->second;
    }

    std::vector<VppCall> vppCallsTo(const std::string& api, const std::string& name)
    {
        SWSS_LOG_ENTER();
        std::vector<VppCall> calls;
        for (const auto& call : g_vppCalls)
        {
            if (call.api == api && call.name == name)
            {
                calls.push_back(call);
            }
        }
        return calls;
    }

    bool loopbackEnabled = false;
    std::string queriedFeature;
    unsigned int loopbackCalls = 0;
    int loopbackAction = -1;
    int loopbackResult = 0;
    int featureResult = 0;
    unsigned int featureCalls = 0;
    bool mockVpp = false;
    int statsResult = 0;
    int lcpResult = 0;
    unsigned int statsCalls = 0;
    unsigned int interfaceCreates = 0;
    uint32_t kernelVrfId = 0;
    int kernelVrfResult = 0;
    int getVrfResult = 0;
    int setVrfResult = 0;
    unsigned int vrfSetCalls = 0;
    std::map<std::string, uint32_t> interfaceVrfs;
    std::set<uint32_t> vrfTables;
    std::map<std::string, vpp_interface_stats_t> interfaceStats;
    std::map<std::string, int> actions;
    std::set<std::string> interfaces;
    std::set<std::string> lcpPairs;
    std::set<std::string> bridgePorts;
}

extern "C" int __wrap_vpp_sonic_ext_feature_get(const char *feature, bool *enabled)
{
    ++featureCalls;
    queriedFeature = feature;
    *enabled = queriedFeature == "iface-loopback" && loopbackEnabled;
    return featureResult;
}

extern "C" int __wrap_vpp_iface_loopback_set_action(const char *name, int action)
{
    ++loopbackCalls;
    loopbackAction = action;
    if (loopbackResult == 0)
    {
        actions[name] = action;
    }
    return loopbackResult;
}

extern "C" int __wrap_vpp_intf_stats_query(const char *name, vpp_interface_stats_t *stats)
{
    ++statsCalls;
    *stats = interfaceStats[name];
    return statsResult;
}

extern "C" int __wrap_create_bvi_interface(uint8_t *, uint32_t instance)
{
    ++interfaceCreates;
    return interfaces.insert("bvi" + std::to_string(instance)).second ? 0 : -1;
}

extern "C" int __wrap_delete_bvi_interface(const char *name)
{
    actions.erase(name);
    g_vppAdminUp.erase(name);
    return interfaces.erase(name) == 1 ? 0 : -1;
}

extern "C" int __wrap_create_sub_interface(const char *parent, uint32_t id, uint16_t vlan)
{
    ++interfaceCreates;
    g_vppCalls.push_back({"create_sub_interface", parent, vlan, id, true});
    return interfaces.insert(std::string(parent) + "." + std::to_string(id)).second ? 0 : -1;
}

extern "C" int __wrap_delete_sub_interface(const char *parent, uint32_t id)
{
    g_vppCalls.push_back({"delete_sub_interface", parent, id, id, false});
    return __wrap_delete_bvi_interface((std::string(parent) + "." + std::to_string(id)).c_str());
}

extern "C" int __wrap_configure_lcp_interface(const char *name, const char *, bool add)
{
    if (add)
    {
        if (lcpResult != 0)
        {
            return lcpResult;
        }
        return lcpPairs.insert(name).second ? 0 : -1;
    }
    return lcpPairs.erase(name) == 1 ? 0 : -1;
}

extern "C" int __wrap_set_sw_interface_l2_bridge(const char *name, uint32_t, bool add, uint32_t)
{
    if (add)
    {
        bridgePorts.insert(name);
    }
    else
    {
        bridgePorts.erase(name);
    }
    return 0;
}

extern "C" int __wrap_refresh_interfaces_list() { return 0; }
extern "C" int __wrap_vpp_want_l2_macs_events2(bool, vpp_mac_event_cb_fn, void *) { return 0; }
extern "C" int __wrap_vpp_ip_flow_hash_set(uint32_t, uint32_t, int) { return 0; }
extern "C" int __wrap_set_interface_vrf(const char *name, uint32_t subId, uint32_t table, bool)
{
    ++vrfSetCalls;
    if (setVrfResult == 0)
    {
        const auto ifname = std::string(name) + (subId ? "." + std::to_string(subId) : "");
        interfaceVrfs[ifname] = table;
    }
    return setVrfResult;
}

extern "C" int __wrap_get_interface_vrf(const char *name, bool, uint32_t *table)
{
    *table = interfaceVrfs[name];
    return getVrfResult;
}

extern "C" int __wrap_ip_vrf_add(uint32_t table, const char *, bool)
{
    vrfTables.insert(table);
    return 0;
}

extern "C" int __wrap_ip_vrf_del(uint32_t table, const char *, bool)
{
    vrfTables.erase(table);
    return 0;
}

extern "C" int __wrap_interface_ip_address_del_all(const char *) { return 0; }

// Keep constructors and CREATE paths real, but isolate VPP threads and host networking.
extern "C" sai_status_t __wrap__ZN5saivs9SwitchVpp17vpp_dp_initializeEv(SwitchVpp *)
{
    return SAI_STATUS_SUCCESS;
}

extern "C" int __wrap__ZN5saivs9SwitchVpp19vs_set_dev_admin_upEPKcb(const char *, bool)
{
    return 0;
}

extern "C" int __real__ZN4swss4execERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEERS5_(
        const std::string&, std::string&);

extern "C" int __wrap__ZN4swss4execERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEERS5_(
        const std::string& command, std::string& output)
{
    if (!mockVpp)
    {
        return __real__ZN4swss4execERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEERS5_(command, output);
    }
    if (command.find("link show") != std::string::npos)
    {
        output = std::to_string(kernelVrfId) + "\n";
        return kernelVrfResult;
    }
    output.clear();
    return 0;
}

extern "C" {

int __wrap_init_vpp_client() { return 0; }
int __wrap_vpp_sync_for_events() { return 0; }
vpp_event_info_t * __wrap_vpp_ev_dequeue() { return NULL; }

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

TEST(SwitchVpp, loopbackActionDisabled)
{
    // Isolate sonicExtFeatureEnabled's process-wide startup-state cache.
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
    ASSERT_EXIT({
        auto status = SwitchVpp::vpp_apply_loopback_action("Ethernet0", SAI_PACKET_ACTION_DROP);
        _exit(status == SAI_STATUS_SUCCESS && loopbackCalls == 0 &&
                queriedFeature == "iface-loopback" ? 0 : 1);
    }, ::testing::ExitedWithCode(0), "");
}

TEST(SwitchVpp, loopbackActionEnabled)
{
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
    ASSERT_EXIT({
        loopbackEnabled = true;
        if (SwitchVpp::vpp_apply_loopback_action("Ethernet0", SAI_PACKET_ACTION_TRAP) != SAI_STATUS_NOT_SUPPORTED ||
                loopbackCalls != 0)
        {
            _exit(1);
        }
        if (SwitchVpp::vpp_apply_loopback_action("Ethernet0", SAI_PACKET_ACTION_DROP) != SAI_STATUS_SUCCESS ||
                loopbackCalls != 1 || loopbackAction != 1 || queriedFeature != "iface-loopback")
        {
            _exit(1);
        }
        if (SwitchVpp::vpp_apply_loopback_action("Ethernet0", SAI_PACKET_ACTION_FORWARD) != SAI_STATUS_SUCCESS ||
                loopbackCalls != 2 || loopbackAction != 0)
        {
            _exit(1);
        }
        loopbackResult = -1;
        _exit(SwitchVpp::vpp_apply_loopback_action("Ethernet0", SAI_PACKET_ACTION_DROP) == SAI_STATUS_FAILURE &&
                loopbackCalls == 3 ? 0 : 1);
    }, ::testing::ExitedWithCode(0), "");
}

TEST(SwitchVpp, loopbackQueryFailureIsRetryable)
{
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
    ASSERT_EXIT({
        featureResult = -1;
        loopbackEnabled = true;
        if (SwitchVpp::vpp_apply_loopback_action("port0", SAI_PACKET_ACTION_DROP) != SAI_STATUS_FAILURE ||
                loopbackCalls != 0)
        {
            _exit(1);
        }
        featureResult = 0;
        _exit(SwitchVpp::vpp_apply_loopback_action("port0", SAI_PACKET_ACTION_DROP) == SAI_STATUS_SUCCESS &&
                loopbackCalls == 1 && loopbackAction == 1 ? 0 : 1);
    }, ::testing::ExitedWithCode(0), "");
}

class SwitchVppRifTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mockVpp = true;
        loopbackEnabled = true;
        featureResult = loopbackResult = statsResult = lcpResult = 0;
        loopbackCalls = statsCalls = interfaceCreates = featureCalls = vrfSetCalls = 0;
        kernelVrfId = 0;
        kernelVrfResult = getVrfResult = setVrfResult = 0;
        interfaceVrfs.clear();
        vrfTables.clear();
        interfaceStats.clear();
        actions.clear();
        interfaces.clear();
        lcpPairs.clear();
        bridgePorts.clear();
        g_vppCalls.clear();
        g_vppAdminUp.clear();

        auto context = ContextConfigContainer::getDefault()->get(0);
        auto config = context->m_scc->getConfig(0);
        config->m_useTapDevice = false;
        config->m_profileMap[SAI_KEY_VS_PORT_CONFIG_FILE] = "/dev/null";
        manager = std::make_shared<RealObjectIdManager>(context->m_guid, context->m_scc);
        switchId = manager->allocateNewSwitchObjectId("");
        sw = std::make_shared<SwitchVpp>(switchId, manager, config);
        config->m_useTapDevice = true;

        port = allocate(SAI_OBJECT_TYPE_PORT);
        lag = allocate(SAI_OBJECT_TYPE_LAG);
        vlan = allocate(SAI_OBJECT_TYPE_VLAN);
        vrf = allocate(SAI_OBJECT_TYPE_VIRTUAL_ROUTER);
        ASSERT_NE(nullptr, sw->m_ifaceRegistry.addPhysicalPort("port0", "Ethernet0", port));
        ASSERT_NE(nullptr, sw->m_ifaceRegistry.addLag(2, 2, lag));
        ASSERT_TRUE(sw->m_ifaceRegistry.setTapName("port0", "tap0"));
        ASSERT_TRUE(sw->m_ifaceRegistry.setTapName("BondEthernet2", "be2"));

        sai_attribute_t attr{};
        attr.id = SAI_SWITCH_ATTR_DEFAULT_VIRTUAL_ROUTER_ID;
        attr.value.oid = vrf;
        ASSERT_EQ(SAI_STATUS_SUCCESS, sw->set_internal(
                SAI_OBJECT_TYPE_SWITCH, sai_serialize_object_id(switchId), &attr));
        attr.id = SAI_VLAN_ATTR_VLAN_ID;
        attr.value.u16 = 100;
        ASSERT_EQ(SAI_STATUS_SUCCESS, sw->create_internal(
                SAI_OBJECT_TYPE_VLAN, sai_serialize_object_id(vlan), switchId, 1, &attr));
    }

    void TearDown() override
    {
        sw->m_switchConfig->m_useTapDevice = false;
        sw.reset();
        mockVpp = false;
    }

    sai_object_id_t allocate(sai_object_type_t type)
    {
        return manager->allocateNewObjectId(type, switchId);
    }

    std::vector<sai_attribute_t> attributes(
            sai_router_interface_type_t type, sai_object_id_t backing,
            int32_t action = SAI_PACKET_ACTION_DROP)
    {
        std::vector<sai_attribute_t> attrs(6);
        attrs[0].id = SAI_ROUTER_INTERFACE_ATTR_TYPE;
        attrs[0].value.s32 = type;
        attrs[1].id = type == SAI_ROUTER_INTERFACE_TYPE_VLAN ?
            SAI_ROUTER_INTERFACE_ATTR_VLAN_ID : SAI_ROUTER_INTERFACE_ATTR_PORT_ID;
        attrs[1].value.oid = backing;
        attrs[2].id = SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID;
        attrs[2].value.oid = vrf;
        attrs[3].id = SAI_ROUTER_INTERFACE_ATTR_SRC_MAC_ADDRESS;
        attrs[3].value.mac[5] = 1;
        attrs[4].id = SAI_ROUTER_INTERFACE_ATTR_LOOPBACK_PACKET_ACTION;
        attrs[4].value.s32 = action;
        attrs[5].id = SAI_ROUTER_INTERFACE_ATTR_OUTER_VLAN_ID;
        attrs[5].value.u16 = 10;
        if (type != SAI_ROUTER_INTERFACE_TYPE_SUB_PORT)
        {
            attrs.pop_back();
        }
        return attrs;
    }

    sai_status_t create(sai_object_id_t rif, const std::vector<sai_attribute_t>& attrs)
    {
        return sw->createRouterif(rif, switchId, static_cast<uint32_t>(attrs.size()), attrs.data());
    }

    std::shared_ptr<RealObjectIdManager> manager;
    std::shared_ptr<SwitchVpp> sw;
    sai_object_id_t switchId, port, lag, vlan, vrf;
};

TEST_F(SwitchVppRifTest, metadataCounterInjectionAndReadsUseBaseImplementation)
{
    auto meta = std::make_shared<saimeta::Meta>(std::make_shared<saimeta::DummySaiInterface>());
    meta->meta_unittests_enable(true);
    sw->setMeta(meta);
    const auto rif = allocate(SAI_OBJECT_TYPE_ROUTER_INTERFACE);
    const sai_stat_id_t id = SAI_ROUTER_INTERFACE_STAT_OUT_ERROR_PACKETS;
    uint64_t values[] = {42, 123456};
    ASSERT_EQ(SAI_STATUS_SUCCESS, sw->getStatsExt(
            SAI_OBJECT_TYPE_ROUTER_INTERFACE, rif, 0x80000001, &id, SAI_STATS_MODE_READ, values));
    values[0] = 0;
    ASSERT_EQ(SAI_STATUS_SUCCESS, sw->getStatsExt(
            SAI_OBJECT_TYPE_ROUTER_INTERFACE, rif, 1, &id, SAI_STATS_MODE_READ_AND_CLEAR, values));
    EXPECT_EQ(42u, values[0]);
    ASSERT_EQ(SAI_STATUS_SUCCESS, sw->getStatsExt(
            SAI_OBJECT_TYPE_ROUTER_INTERFACE, rif, 1, &id, SAI_STATS_MODE_READ, values));
    EXPECT_EQ(0u, values[0]);
    EXPECT_EQ(123456u, values[1]);
    EXPECT_EQ(0u, statsCalls);
}

TEST_F(SwitchVppRifTest, featureQueryErrorsDoNotPersistCreateOrSet)
{
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
    ASSERT_EXIT({
        auto attrs = attributes(SAI_ROUTER_INTERFACE_TYPE_PORT, port);
        const auto rif = allocate(SAI_OBJECT_TYPE_ROUTER_INTERFACE);
        featureResult = -1;
        if (create(rif, attrs) != SAI_STATUS_FAILURE ||
                sw->m_objectHash.at(SAI_OBJECT_TYPE_ROUTER_INTERFACE).count(sai_serialize_object_id(rif)) != 0)
        {
            _exit(1);
        }
        const auto action = attrs.back();
        attrs.pop_back();
        if (create(rif, attrs) != SAI_STATUS_SUCCESS ||
                sw->vpp_update_router_interface(rif, 1, &action) != SAI_STATUS_FAILURE ||
                loopbackCalls != 0)
        {
            _exit(1);
        }
        const auto& stored = sw->m_objectHash.at(SAI_OBJECT_TYPE_ROUTER_INTERFACE).at(sai_serialize_object_id(rif));
        if (stored.count("SAI_ROUTER_INTERFACE_ATTR_LOOPBACK_PACKET_ACTION") != 0)
        {
            _exit(1);
        }
        featureResult = 0;
        if (sw->vpp_update_router_interface(rif, 1, &action) != SAI_STATUS_SUCCESS)
        {
            _exit(1);
        }
        sai_attribute_t value{};
        value.id = SAI_ROUTER_INTERFACE_ATTR_LOOPBACK_PACKET_ACTION;
        _exit(sw->get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, rif, 1, &value) == SAI_STATUS_SUCCESS &&
                value.value.s32 == SAI_PACKET_ACTION_DROP && loopbackCalls == 1 ? 0 : 1);
    }, ::testing::ExitedWithCode(0), "");
}

TEST_F(SwitchVppRifTest, persistentInterfaceCountersStartAtEachRifCreation)
{
    const sai_stat_id_t ids[] = {
        SAI_ROUTER_INTERFACE_STAT_IN_PACKETS, SAI_ROUTER_INTERFACE_STAT_IN_OCTETS,
        SAI_ROUTER_INTERFACE_STAT_OUT_PACKETS, SAI_ROUTER_INTERFACE_STAT_OUT_OCTETS,
        SAI_ROUTER_INTERFACE_STAT_IN_ERROR_PACKETS, SAI_ROUTER_INTERFACE_STAT_OUT_ERROR_PACKETS,
    };
    for (auto backing : {port, lag})
    {
        const auto name = sw->m_ifaceRegistry.resolveHwIfName(backing, 0);
        auto& stats = interfaceStats[name];
        stats.rx = 10;
        stats.rx_bytes = 100;
        stats.tx = 20;
        stats.tx_bytes = 200;
        stats.rx_error = 3;
        stats.tx_error = 4;
        const auto attrs = attributes(SAI_ROUTER_INTERFACE_TYPE_PORT, backing);
        auto rif = allocate(SAI_OBJECT_TYPE_ROUTER_INTERFACE);
        ASSERT_EQ(SAI_STATUS_SUCCESS, create(rif, attrs));
        uint64_t values[6]{};
        ASSERT_EQ(SAI_STATUS_SUCCESS, sw->getRifStatsExt(rif, 6, ids, SAI_STATS_MODE_READ, values));
        for (auto value : values)
        {
            EXPECT_EQ(0u, value);
        }
        stats.tx_error += 7;
        ASSERT_EQ(SAI_STATUS_SUCCESS, sw->getRifStatsExt(rif, 6, ids, SAI_STATS_MODE_READ, values));
        EXPECT_EQ(7u, values[5]);
        ASSERT_EQ(SAI_STATUS_SUCCESS, sw->removeRouterif(rif));
        EXPECT_EQ(0u, sw->m_rifStatsBaseMap.count(rif));
        ASSERT_NE(nullptr, sw->m_ifaceRegistry.findByOid(backing));
        rif = allocate(SAI_OBJECT_TYPE_ROUTER_INTERFACE);
        ASSERT_EQ(SAI_STATUS_SUCCESS, create(rif, attrs));
        ASSERT_EQ(SAI_STATUS_SUCCESS, sw->getRifStatsExt(rif, 6, ids, SAI_STATS_MODE_READ, values));
        EXPECT_EQ(0u, values[5]);
    }
}

TEST_F(SwitchVppRifTest, failedNonDefaultVrfCreateRestoresPortAndLagBeforeDefaultRetry)
{
    for (auto backing : {port, lag})
    {
        const auto name = sw->m_ifaceRegistry.resolveHwIfName(backing, 0);
        for (bool failSnapshot : {false, true})
        {
            SCOPED_TRACE(name + (failSnapshot ? " snapshot" : " action"));
            const auto rif = allocate(SAI_OBJECT_TYPE_ROUTER_INTERFACE);
            const auto nonDefaultVrf = allocate(SAI_OBJECT_TYPE_VIRTUAL_ROUTER);
            auto attrs = attributes(SAI_ROUTER_INTERFACE_TYPE_PORT, backing);
            attrs[2].value.oid = nonDefaultVrf;
            kernelVrfId = 100;
            interfaceVrfs[name] = 0;
            loopbackResult = failSnapshot ? 0 : -1;
            statsResult = failSnapshot ? -1 : 0;
            ASSERT_EQ(SAI_STATUS_FAILURE, create(rif, attrs));
            EXPECT_EQ(0u, interfaceVrfs.at(name));
            EXPECT_NE(nullptr, sw->m_ifaceRegistry.findByOid(backing));
            EXPECT_TRUE(sw->m_ifaceRegistry.resolveHwIfByRifOid(rif).empty());
            EXPECT_EQ(1u, vrfTables.count(100));
            EXPECT_EQ(1u, sw->vrf_objMap.count(nonDefaultVrf));

            kernelVrfId = 0;
            loopbackResult = statsResult = 0;
            attrs[2].value.oid = vrf;
            ASSERT_EQ(SAI_STATUS_SUCCESS, create(rif, attrs));
            EXPECT_EQ(0u, interfaceVrfs.at(name));
            ASSERT_EQ(SAI_STATUS_SUCCESS, sw->removeRouterif(rif));
        }
    }
}

TEST_F(SwitchVppRifTest, rollbackRestoresActualPreviousVrfAndDefaultCreateBindsTableZero)
{
    const auto rif = allocate(SAI_OBJECT_TYPE_ROUTER_INTERFACE);
    auto attrs = attributes(SAI_ROUTER_INTERFACE_TYPE_PORT, port);
    attrs[2].value.oid = allocate(SAI_OBJECT_TYPE_VIRTUAL_ROUTER);
    interfaceVrfs["port0"] = 42;
    vrfTables.insert(42);
    kernelVrfId = 100;
    statsResult = -1;
    ASSERT_EQ(SAI_STATUS_FAILURE, create(rif, attrs));
    EXPECT_EQ(42u, interfaceVrfs.at("port0"));
    EXPECT_EQ(1u, vrfTables.count(42));
    EXPECT_EQ(1u, vrfTables.count(100));

    kernelVrfId = 0;
    statsResult = 0;
    attrs[2].value.oid = vrf;
    ASSERT_EQ(SAI_STATUS_SUCCESS, create(rif, attrs));
    EXPECT_EQ(0u, interfaceVrfs.at("port0"));
}

TEST_F(SwitchVppRifTest, vrfLookupAndProgrammingErrorsAbortBeforeActionProgramming)
{
    for (const std::string failure : {"kernel", "get", "set"})
    {
        SCOPED_TRACE(failure);
        const auto rif = allocate(SAI_OBJECT_TYPE_ROUTER_INTERFACE);
        auto attrs = attributes(SAI_ROUTER_INTERFACE_TYPE_PORT, port);
        attrs[2].value.oid = allocate(SAI_OBJECT_TYPE_VIRTUAL_ROUTER);
        kernelVrfId = 100;
        interfaceVrfs["port0"] = 42;
        kernelVrfResult = failure == "kernel" ? -1 : 0;
        getVrfResult = failure == "get" ? -1 : 0;
        setVrfResult = failure == "set" ? -1 : 0;
        vrfSetCalls = loopbackCalls = 0;
        ASSERT_EQ(SAI_STATUS_FAILURE, create(rif, attrs));
        EXPECT_EQ(42u, interfaceVrfs.at("port0"));
        EXPECT_EQ(0u, loopbackCalls);
        EXPECT_EQ(failure == "set" ? 1u : 0u, vrfSetCalls);
        EXPECT_TRUE(sw->m_ifaceRegistry.resolveHwIfByRifOid(rif).empty());
    }
}

TEST_F(SwitchVppRifTest, loopbackCapabilityMatchesEnabledAndDisabledFeature)
{
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
    for (bool enabled : {false, true})
    {
        ASSERT_EXIT({
            loopbackEnabled = enabled;
            sai_attr_capability_t capability{};
            const auto status = sw->queryAttributeCapability(switchId, SAI_OBJECT_TYPE_ROUTER_INTERFACE,
                    SAI_ROUTER_INTERFACE_ATTR_LOOPBACK_PACKET_ACTION, &capability);
            _exit(status == SAI_STATUS_SUCCESS && capability.create_implemented == enabled &&
                    capability.set_implemented == enabled && capability.get_implemented == enabled &&
                    queriedFeature == "iface-loopback" && featureCalls == 1 ? 0 : 1);
        }, ::testing::ExitedWithCode(0), "");
    }
}

TEST_F(SwitchVppRifTest, loopbackCapabilityQueryFailureIsRetryable)
{
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
    ASSERT_EXIT({
        sai_attr_capability_t capability{};
        featureResult = -1;
        if (sw->queryAttributeCapability(switchId, SAI_OBJECT_TYPE_ROUTER_INTERFACE,
                    SAI_ROUTER_INTERFACE_ATTR_LOOPBACK_PACKET_ACTION, &capability) != SAI_STATUS_FAILURE)
        {
            _exit(1);
        }
        featureResult = 0;
        const auto status = sw->queryAttributeCapability(switchId, SAI_OBJECT_TYPE_ROUTER_INTERFACE,
                SAI_ROUTER_INTERFACE_ATTR_LOOPBACK_PACKET_ACTION, &capability);
        _exit(status == SAI_STATUS_SUCCESS && capability.create_implemented &&
                capability.set_implemented && capability.get_implemented && featureCalls == 2 ? 0 : 1);
    }, ::testing::ExitedWithCode(0), "");
}

TEST_F(SwitchVppRifTest, unrelatedCapabilitiesDoNotQueryLoopbackFeature)
{
    featureResult = -1;
    sai_attr_capability_t capability{};
    ASSERT_EQ(SAI_STATUS_SUCCESS, sw->queryAttributeCapability(switchId, SAI_OBJECT_TYPE_ROUTER_INTERFACE,
            SAI_ROUTER_INTERFACE_ATTR_TYPE, &capability));
    EXPECT_TRUE(capability.create_implemented && capability.set_implemented && capability.get_implemented);
    ASSERT_EQ(SAI_STATUS_SUCCESS, sw->queryAttributeCapability(switchId, SAI_OBJECT_TYPE_PORT,
            SAI_PORT_ATTR_MTU, &capability));
    EXPECT_TRUE(capability.create_implemented && capability.set_implemented && capability.get_implemented);
    EXPECT_EQ(0u, featureCalls);
}

TEST_F(SwitchVppRifTest, queryFailureDoesNotAcknowledgeClearOrChangeBaseline)
{
    const auto rif = allocate(SAI_OBJECT_TYPE_ROUTER_INTERFACE);
    interfaceStats["port0"].tx = 100;
    ASSERT_EQ(SAI_STATUS_SUCCESS, create(rif, attributes(SAI_ROUTER_INTERFACE_TYPE_PORT, port)));
    const auto baseline = sw->m_rifStatsBaseMap.at(rif);
    const sai_stat_id_t id = SAI_ROUTER_INTERFACE_STAT_OUT_PACKETS;
    uint64_t value = 999;
    statsResult = -1;
    ASSERT_EQ(SAI_STATUS_FAILURE, sw->getRifStatsExt(rif, 1, &id, SAI_STATS_MODE_READ_AND_CLEAR, &value));
    EXPECT_EQ(999u, value);
    EXPECT_EQ(baseline, sw->m_rifStatsBaseMap.at(rif));
    statsResult = 0;
    interfaceStats["port0"].tx = 130;
    ASSERT_EQ(SAI_STATUS_SUCCESS, sw->getRifStatsExt(rif, 1, &id, SAI_STATS_MODE_READ_AND_CLEAR, &value));
    EXPECT_EQ(30u, value);
    ASSERT_EQ(SAI_STATUS_SUCCESS, sw->getRifStatsExt(rif, 1, &id, SAI_STATS_MODE_READ, &value));
    EXPECT_EQ(0u, value);
}

TEST_F(SwitchVppRifTest, resetDiscardsOnlyTheAffectedBaseline)
{
    const auto rif = allocate(SAI_OBJECT_TYPE_ROUTER_INTERFACE);
    interfaceStats["port0"].tx = 100;
    interfaceStats["port0"].tx_error = 30;
    ASSERT_EQ(SAI_STATUS_SUCCESS, create(rif, attributes(SAI_ROUTER_INTERFACE_TYPE_PORT, port)));
    const sai_stat_id_t ids[] = {
        SAI_ROUTER_INTERFACE_STAT_OUT_PACKETS, SAI_ROUTER_INTERFACE_STAT_OUT_ERROR_PACKETS};
    uint64_t values[2]{};
    interfaceStats["port0"].tx = 5;
    interfaceStats["port0"].tx_error = 35;
    ASSERT_EQ(SAI_STATUS_SUCCESS, sw->getRifStatsExt(rif, 2, ids, SAI_STATS_MODE_READ, values));
    EXPECT_EQ(5u, values[0]);
    EXPECT_EQ(5u, values[1]);
    interfaceStats["port0"].tx = 101;
    ASSERT_EQ(SAI_STATUS_SUCCESS, sw->getRifStatsExt(rif, 2, ids, SAI_STATS_MODE_READ_AND_CLEAR, values));
    EXPECT_EQ(101u, values[0]);
    ASSERT_EQ(SAI_STATUS_SUCCESS, sw->getRifStatsExt(rif, 2, ids, SAI_STATS_MODE_READ, values));
    EXPECT_EQ(0u, values[0]);
    EXPECT_EQ(0u, values[1]);
}

TEST_F(SwitchVppRifTest, unresolvedRifDoesNotAllocateBaseline)
{
    const auto rif = allocate(SAI_OBJECT_TYPE_ROUTER_INTERFACE);
    const sai_stat_id_t id = SAI_ROUTER_INTERFACE_STAT_OUT_PACKETS;
    uint64_t value = 1;
    EXPECT_EQ(SAI_STATUS_SUCCESS, sw->getRifStatsExt(rif, 1, &id, SAI_STATS_MODE_READ_AND_CLEAR, &value));
    EXPECT_EQ(0u, value);
    EXPECT_EQ(0u, sw->m_rifStatsBaseMap.count(rif));
    EXPECT_EQ(0u, statsCalls);
}

TEST_F(SwitchVppRifTest, unsupportedActionsFailBeforeAllocatingInterfaces)
{
    for (auto type : {SAI_ROUTER_INTERFACE_TYPE_VLAN, SAI_ROUTER_INTERFACE_TYPE_SUB_PORT})
    {
        const auto rif = allocate(SAI_OBJECT_TYPE_ROUTER_INTERFACE);
        EXPECT_EQ(SAI_STATUS_NOT_SUPPORTED, create(rif, attributes(
                type, type == SAI_ROUTER_INTERFACE_TYPE_VLAN ? vlan : port, SAI_PACKET_ACTION_TRAP)));
        EXPECT_TRUE(sw->m_ifaceRegistry.resolveHwIfByRifOid(rif).empty());
    }
    EXPECT_EQ(0u, interfaceCreates);
    EXPECT_EQ(0u, loopbackCalls);
}

TEST_F(SwitchVppRifTest, failedCreatesUnwindOwnedResourcesAndCanBeRetried)
{
    const auto parentRif = allocate(SAI_OBJECT_TYPE_ROUTER_INTERFACE);
    ASSERT_TRUE(sw->m_ifaceRegistry.setRifOid("port0", parentRif));
    actions["port0"] = 1;
    for (auto type : {SAI_ROUTER_INTERFACE_TYPE_VLAN, SAI_ROUTER_INTERFACE_TYPE_SUB_PORT})
    {
        const std::string name = type == SAI_ROUTER_INTERFACE_TYPE_VLAN ? "bvi100" : "port0.10";
        const auto attrs = attributes(type, type == SAI_ROUTER_INTERFACE_TYPE_VLAN ? vlan : port);
        for (const std::string failure : {"action", "snapshot", "lcp"})
        {
            SCOPED_TRACE(name + " " + failure);
            const auto rif = allocate(SAI_OBJECT_TYPE_ROUTER_INTERFACE);
            loopbackResult = failure == "action" ? -1 : 0;
            statsResult = failure == "snapshot" ? -1 : 0;
            lcpResult = failure == "lcp" ? -1 : 0;
            ASSERT_EQ(SAI_STATUS_FAILURE, create(rif, attrs));
            EXPECT_EQ(0u, interfaces.count(name));
            EXPECT_EQ(0u, lcpPairs.count(name));
            EXPECT_EQ(0u, bridgePorts.count(name));
            EXPECT_EQ(nullptr, sw->m_ifaceRegistry.findByHwif(name));
            EXPECT_TRUE(sw->m_ifaceRegistry.resolveHwIfByRifOid(rif).empty());
            EXPECT_EQ(0u, sw->m_rifStatsBaseMap.count(rif));
            EXPECT_EQ(0u, sw->m_objectHash.at(SAI_OBJECT_TYPE_ROUTER_INTERFACE).count(sai_serialize_object_id(rif)));
            EXPECT_EQ("port0", sw->m_ifaceRegistry.resolveHwIfByRifOid(parentRif));
            EXPECT_EQ(1, actions.at("port0"));
            EXPECT_EQ(1u, sw->m_objectHash.at(SAI_OBJECT_TYPE_VLAN).count(sai_serialize_object_id(vlan)));
            EXPECT_EQ(1u, sw->vrf_objMap.count(vrf));

            loopbackResult = statsResult = lcpResult = 0;
            ASSERT_EQ(SAI_STATUS_SUCCESS, create(rif, attrs));
            ASSERT_EQ(SAI_STATUS_SUCCESS, sw->removeRouterif(rif));
        }
    }
}

TEST_F(SwitchVppRifTest, failedAllocationDoesNotDeleteExistingInterfaces)
{
    for (auto type : {SAI_ROUTER_INTERFACE_TYPE_VLAN, SAI_ROUTER_INTERFACE_TYPE_SUB_PORT})
    {
        const std::string name = type == SAI_ROUTER_INTERFACE_TYPE_VLAN ? "bvi100" : "port0.10";
        interfaces.insert(name);
        lcpPairs.insert(name);
        const auto rif = allocate(SAI_OBJECT_TYPE_ROUTER_INTERFACE);
        EXPECT_EQ(SAI_STATUS_FAILURE, create(rif, attributes(
                type, type == SAI_ROUTER_INTERFACE_TYPE_VLAN ? vlan : port)));
        EXPECT_EQ(1u, interfaces.count(name));
        EXPECT_EQ(1u, lcpPairs.count(name));
    }
}

TEST_F(SwitchVppRifTest, failedPortSnapshotRestoresActionWithoutDeletingPort)
{
    const auto rif = allocate(SAI_OBJECT_TYPE_ROUTER_INTERFACE);
    statsResult = -1;
    ASSERT_EQ(SAI_STATUS_FAILURE, create(rif, attributes(SAI_ROUTER_INTERFACE_TYPE_PORT, port)));
    EXPECT_EQ(0, actions.at("port0"));
    EXPECT_NE(nullptr, sw->m_ifaceRegistry.findByOid(port));
    EXPECT_TRUE(sw->m_ifaceRegistry.resolveHwIfByRifOid(rif).empty());
    EXPECT_EQ(0u, sw->m_rifStatsBaseMap.count(rif));
}

TEST(VppInterfaceRegistry, rifLookupUsesSeparateIndexForEveryInterfaceType)
{
    VppInterfaceRegistry registry;
    auto port = registry.addPhysicalPort("TenGigabitEthernet0/0/0", "Ethernet0", 1);
    auto lag = registry.addLag(2, 20, 2);
    auto bvi = registry.addBvi(100, 3);
    ASSERT_NE(nullptr, port);
    ASSERT_NE(nullptr, lag);
    ASSERT_NE(nullptr, bvi);
    ASSERT_NE(nullptr, registry.addSubInterface(port, 10, 10));
    ASSERT_NE(nullptr, registry.addSubInterface(lag, 20, 20));

    const std::vector<std::string> names = {
        "TenGigabitEthernet0/0/0", "BondEthernet2", "bvi100",
        "TenGigabitEthernet0/0/0.10", "BondEthernet2.20"
    };
    sai_object_id_t rif = 100;
    for (const auto& name : names)
    {
        ASSERT_TRUE(registry.setRifOid(name, rif));
        EXPECT_EQ(name, registry.resolveHwIfByRifOid(rif));
        EXPECT_EQ(nullptr, registry.findByOid(rif));
        ++rif;
    }
    EXPECT_EQ(port, registry.findByOid(1));
    EXPECT_EQ(lag, registry.findByOid(2));
    EXPECT_EQ(bvi, registry.findByOid(3));
}

TEST(VppInterfaceRegistry, rifBindingRejectsAliasesAndClearsPersistentInterfaces)
{
    VppInterfaceRegistry registry;
    auto port = registry.addPhysicalPort("port0", "Ethernet0", 1);
    auto lag = registry.addLag(2, 20, 2);
    ASSERT_NE(nullptr, port);
    ASSERT_NE(nullptr, lag);
    EXPECT_FALSE(registry.setRifOid("missing", 100));
    EXPECT_FALSE(registry.setRifOid("port0", SAI_NULL_OBJECT_ID));
    ASSERT_TRUE(registry.setRifOid("port0", 100));
    EXPECT_FALSE(registry.setRifOid("BondEthernet2", 100));
    EXPECT_EQ("port0", registry.resolveHwIfByRifOid(100));

    ASSERT_TRUE(registry.setRifOid("port0", 101));
    EXPECT_TRUE(registry.resolveHwIfByRifOid(100).empty());
    ASSERT_TRUE(registry.clearRifOid(101));
    EXPECT_FALSE(registry.clearRifOid(101));
    EXPECT_TRUE(registry.resolveHwIfByRifOid(101).empty());
    EXPECT_FALSE(port->hasRifOid());
    EXPECT_EQ(port, registry.findByOid(1));
    ASSERT_TRUE(registry.setRifOid("BondEthernet2", 101));
    EXPECT_EQ("BondEthernet2", registry.resolveHwIfByRifOid(101));
}

TEST(VppInterfaceRegistry, removingSubInterfaceLeavesParentRifBinding)
{
    VppInterfaceRegistry registry;
    auto port = registry.addPhysicalPort("port0", "Ethernet0", 1);
    ASSERT_NE(nullptr, registry.addSubInterface(port, 10, 10));
    ASSERT_TRUE(registry.setRifOid("port0", 100));
    ASSERT_TRUE(registry.setRifOid("port0.10", 101));
    EXPECT_EQ(1u, registry.remove("port0.10"));
    EXPECT_TRUE(registry.resolveHwIfByRifOid(101).empty());
    EXPECT_EQ("port0", registry.resolveHwIfByRifOid(100));
    ASSERT_NE(nullptr, registry.addSubInterface(port, 10, 10));
    ASSERT_TRUE(registry.setRifOid("port0.10", 101));
    EXPECT_EQ(2u, registry.removeByOid(1));
    EXPECT_TRUE(registry.resolveHwIfByRifOid(100).empty());
    EXPECT_TRUE(registry.resolveHwIfByRifOid(101).empty());
}

TEST(VppInterfaceRegistry, clearingRegistryDoesNotRetainRecycledRifOid)
{
    VppInterfaceRegistry registry;
    ASSERT_NE(nullptr, registry.addBvi(100, 1));
    ASSERT_TRUE(registry.setRifOid("bvi100", 100));
    registry.clear();
    EXPECT_TRUE(registry.resolveHwIfByRifOid(100).empty());
    ASSERT_NE(nullptr, registry.addBvi(200, 2));
    ASSERT_TRUE(registry.setRifOid("bvi200", 100));
    EXPECT_EQ("bvi200", registry.resolveHwIfByRifOid(100));
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
            interfaces.clear();
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
