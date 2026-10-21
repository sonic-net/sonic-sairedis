#include "MockVppXlate.h"
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

TEST(SwitchVpp, loopbackActionDisabled)
{
    // Isolate sonicExtFeatureEnabled's process-wide startup-state cache.
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
    ASSERT_EXIT({
        auto status = SwitchVpp::vpp_apply_loopback_action("Ethernet0", SAI_PACKET_ACTION_DROP);
        _exit(status == SAI_STATUS_SUCCESS && vppMock().loopbackCalls == 0 &&
                vppMock().queriedFeature == "iface-loopback" ? 0 : 1);
    }, ::testing::ExitedWithCode(0), "");
}

TEST(SwitchVpp, loopbackActionEnabled)
{
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
    ASSERT_EXIT({
        vppMock().loopbackEnabled = true;
        if (SwitchVpp::vpp_apply_loopback_action("Ethernet0", SAI_PACKET_ACTION_TRAP) != SAI_STATUS_NOT_SUPPORTED ||
                vppMock().loopbackCalls != 0)
        {
            _exit(1);
        }
        if (SwitchVpp::vpp_apply_loopback_action("Ethernet0", SAI_PACKET_ACTION_DROP) != SAI_STATUS_SUCCESS ||
                vppMock().loopbackCalls != 1 || vppMock().loopbackAction != 1 || vppMock().queriedFeature != "iface-loopback")
        {
            _exit(1);
        }
        if (SwitchVpp::vpp_apply_loopback_action("Ethernet0", SAI_PACKET_ACTION_FORWARD) != SAI_STATUS_SUCCESS ||
                vppMock().loopbackCalls != 2 || vppMock().loopbackAction != 0)
        {
            _exit(1);
        }
        vppMock().loopbackResult = -1;
        _exit(SwitchVpp::vpp_apply_loopback_action("Ethernet0", SAI_PACKET_ACTION_DROP) == SAI_STATUS_FAILURE &&
                vppMock().loopbackCalls == 3 ? 0 : 1);
    }, ::testing::ExitedWithCode(0), "");
}

TEST(SwitchVpp, loopbackQueryFailureIsRetryable)
{
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
    ASSERT_EXIT({
        vppMock().featureResult = -1;
        vppMock().loopbackEnabled = true;
        if (SwitchVpp::vpp_apply_loopback_action("port0", SAI_PACKET_ACTION_DROP) != SAI_STATUS_FAILURE ||
                vppMock().loopbackCalls != 0)
        {
            _exit(1);
        }
        vppMock().featureResult = 0;
        _exit(SwitchVpp::vpp_apply_loopback_action("port0", SAI_PACKET_ACTION_DROP) == SAI_STATUS_SUCCESS &&
                vppMock().loopbackCalls == 1 && vppMock().loopbackAction == 1 ? 0 : 1);
    }, ::testing::ExitedWithCode(0), "");
}

class SwitchVppRifTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        vppMock().reset();
        vppMock().loopbackEnabled = true;

        auto context = ContextConfigContainer::getDefault()->get(0);
        auto config = context->m_scc->getConfig(0);
        config->m_useTapDevice = false;
        config->m_profileMap[SAI_KEY_VS_PORT_CONFIG_FILE] = "/dev/null";
        manager = std::make_shared<RealObjectIdManager>(context->m_guid, context->m_scc);
        switchId = manager->allocateNewSwitchObjectId("");
        sw = std::make_shared<TestableSwitchVpp>(switchId, manager, config);
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
    }

    sai_object_id_t allocate(sai_object_type_t type)
    {
        SWSS_LOG_ENTER();
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
        SWSS_LOG_ENTER();
        return sw->createRouterif(rif, switchId, static_cast<uint32_t>(attrs.size()), attrs.data());
    }

    std::vector<sai_attribute_t> settingsAttributes(sai_object_id_t backing, bool enabled)
    {
        SWSS_LOG_ENTER();
        auto attrs = attributes(SAI_ROUTER_INTERFACE_TYPE_PORT, backing);
        sai_attribute_t attr{};
        attr.id = SAI_ROUTER_INTERFACE_ATTR_MTU;
        attr.value.u32 = 9100;
        attrs.push_back(attr);
        attr.id = SAI_ROUTER_INTERFACE_ATTR_ADMIN_MPLS_STATE;
        attr.value.booldata = enabled;
        attrs.push_back(attr);
        attr.id = SAI_ROUTER_INTERFACE_ATTR_ADMIN_V4_STATE;
        attrs.push_back(attr);
        return attrs;
    }

    std::shared_ptr<RealObjectIdManager> manager;
    std::shared_ptr<SwitchVpp> sw;
    sai_object_id_t switchId, port, lag, vlan, vrf;
};

TEST_F(SwitchVppRifTest, failedPersistentSnapshotRestoresAllSettingsBeforeRetry)
{
    const std::array<uint32_t, 4> previousMtu{{1500, 1400, 1300, 1200}};
    for (auto backing : {port, lag})
    {
        const std::string name = backing == port ? "port0" : "BondEthernet2";
        const bool previousEnabled = backing == port;
        vppMock().mtus[name] = previousMtu;
        vppMock().mplsReferences[name] = previousEnabled;
        vppMock().adminUp[name] = previousEnabled;
        vppMock().statsResult = -1;
        auto rif = allocate(SAI_OBJECT_TYPE_ROUTER_INTERFACE);
        EXPECT_EQ(SAI_STATUS_FAILURE, create(rif, settingsAttributes(backing, !previousEnabled)));
        EXPECT_EQ(previousMtu, vppMock().mtus[name]);
        EXPECT_EQ(previousEnabled, vppMock().mplsReferences[name]);
        EXPECT_EQ(previousEnabled, vppMock().isAdminUp(name));
        EXPECT_TRUE(sw->m_ifaceRegistry.resolveHwIfByRifOid(rif).empty());
        EXPECT_EQ(0u, sw->m_rifStatsBaseMap.count(rif));

        vppMock().statsResult = 0;
        ASSERT_EQ(SAI_STATUS_SUCCESS, create(rif, attributes(SAI_ROUTER_INTERFACE_TYPE_PORT, backing)));
        EXPECT_EQ(previousMtu, vppMock().mtus[name]);
        EXPECT_EQ(previousEnabled, vppMock().mplsReferences[name]);
        EXPECT_EQ(previousEnabled, vppMock().isAdminUp(name));
        ASSERT_EQ(SAI_STATUS_SUCCESS, sw->removeRouterif(rif));
    }
}

TEST_F(SwitchVppRifTest, settingsQueryAndSetterErrorsFailCreateWithoutChangingPriorState)
{
    const std::array<uint32_t, 4> previousMtu{{1500, 1400, 1300, 1200}};
    for (int *failure : {&vppMock().settingsResult, &vppMock().mplsQueryResult,
                        &vppMock().mtuResult, &vppMock().mplsResult, &vppMock().adminResult})
    {
        vppMock().mtus["port0"] = previousMtu;
        vppMock().mplsReferences["port0"] = 0;
        vppMock().adminUp["port0"] = false;
        *failure = -1;
        auto rif = allocate(SAI_OBJECT_TYPE_ROUTER_INTERFACE);
        EXPECT_EQ(SAI_STATUS_FAILURE, create(rif, settingsAttributes(port, true)));
        EXPECT_EQ(previousMtu, vppMock().mtus["port0"]);
        EXPECT_EQ(0u, vppMock().mplsReferences["port0"]);
        EXPECT_FALSE(vppMock().isAdminUp("port0"));
        EXPECT_TRUE(sw->m_ifaceRegistry.resolveHwIfByRifOid(rif).empty());
        *failure = 0;
    }
}

TEST_F(SwitchVppRifTest, matchingMplsStatePreservesReferencesOnFailureAndSuccess)
{
    for (auto backing : {port, lag})
    {
        const std::string name = backing == port ? "port0" : "BondEthernet2";
        for (unsigned int references : {0u, 1u, 2u})
        {
            vppMock().mplsReferences[name] = references;
            auto attrs = settingsAttributes(backing, references != 0);
            auto rif = allocate(SAI_OBJECT_TYPE_ROUTER_INTERFACE);
            vppMock().statsResult = -1;
            EXPECT_EQ(SAI_STATUS_FAILURE, create(rif, attrs));
            EXPECT_EQ(references, vppMock().mplsReferences[name]);

            vppMock().statsResult = 0;
            ASSERT_EQ(SAI_STATUS_SUCCESS, create(rif, attrs));
            EXPECT_EQ(references, vppMock().mplsReferences[name]);
            ASSERT_EQ(SAI_STATUS_SUCCESS, sw->removeRouterif(rif));
        }
    }

    auto attrs = attributes(SAI_ROUTER_INTERFACE_TYPE_SUB_PORT, port);
    sai_attribute_t mpls{};
    mpls.id = SAI_ROUTER_INTERFACE_ATTR_ADMIN_MPLS_STATE;
    mpls.value.booldata = false;
    attrs.push_back(mpls);
    auto rif = allocate(SAI_OBJECT_TYPE_ROUTER_INTERFACE);
    ASSERT_EQ(SAI_STATUS_SUCCESS, create(rif, attrs));
    EXPECT_EQ(0u, vppMock().mplsReferences["port0.10"]);
}

TEST_F(SwitchVppRifTest, mplsPostconditionFailuresRestoreReferencesAndSettings)
{
    const std::array<uint32_t, 4> previousMtu{{1500, 1400, 1300, 1200}};
    for (auto backing : {port, lag})
    {
        const std::string name = backing == port ? "port0" : "BondEthernet2";
        for (bool queryFailure : {false, true})
        {
            const unsigned int references = queryFailure ? 1 : 2;
            vppMock().mtus[name] = previousMtu;
            vppMock().mplsReferences[name] = references;
            vppMock().mplsQueryCalls = 0;
            vppMock().mplsQueryFailOnCall = queryFailure ? 2 : 0;
            auto rif = allocate(SAI_OBJECT_TYPE_ROUTER_INTERFACE);
            ASSERT_EQ(SAI_STATUS_FAILURE, create(rif, settingsAttributes(backing, false)));
            EXPECT_EQ(references, vppMock().mplsReferences[name]);
            EXPECT_EQ(previousMtu, vppMock().mtus[name]);
            EXPECT_TRUE(sw->m_ifaceRegistry.resolveHwIfByRifOid(rif).empty());
            EXPECT_EQ(0u, sw->m_rifStatsBaseMap.count(rif));
        }
    }
}

TEST_F(SwitchVppRifTest, failedParentCreateDoesNotConsumeDeferredChildAdmin)
{
    sw->m_sub_if_admin_up_pending.insert("port0.10");
    vppMock().statsResult = -1;
    auto rif = allocate(SAI_OBJECT_TYPE_ROUTER_INTERFACE);
    EXPECT_EQ(SAI_STATUS_FAILURE, create(rif, settingsAttributes(port, true)));
    EXPECT_FALSE(vppMock().isAdminUp("port0"));
    EXPECT_FALSE(vppMock().isAdminUp("port0.10"));
    EXPECT_EQ(1u, sw->m_sub_if_admin_up_pending.count("port0.10"));

    vppMock().statsResult = 0;
    ASSERT_EQ(SAI_STATUS_SUCCESS, create(rif, settingsAttributes(port, true)));
    EXPECT_TRUE(vppMock().isAdminUp("port0"));
    EXPECT_TRUE(vppMock().isAdminUp("port0.10"));
    EXPECT_TRUE(sw->m_sub_if_admin_up_pending.empty());
}

TEST_F(SwitchVppRifTest, failedSubPortCreateDoesNotApplyDeferredAdminToReplacement)
{
    auto attrs = attributes(SAI_ROUTER_INTERFACE_TYPE_SUB_PORT, port);
    sai_attribute_t admin{};
    admin.id = SAI_ROUTER_INTERFACE_ATTR_ADMIN_V4_STATE;
    admin.value.booldata = true;
    attrs.push_back(admin);
    auto rif = allocate(SAI_OBJECT_TYPE_ROUTER_INTERFACE);
    vppMock().statsResult = -1;
    EXPECT_EQ(SAI_STATUS_FAILURE, create(rif, attrs));
    EXPECT_EQ(0u, sw->m_sub_if_admin_up_pending.count("port0.10"));
    EXPECT_EQ(0u, vppMock().interfaces.count("port0.10"));

    attrs.pop_back();
    vppMock().statsResult = 0;
    ASSERT_EQ(SAI_STATUS_SUCCESS, create(rif, attrs));
    const auto calls = vppMock().callsTo("interface_set_state", "port0.10").size();
    ASSERT_EQ(SAI_STATUS_SUCCESS, sw->vpp_set_interface_state(port, 0, true));
    EXPECT_FALSE(vppMock().isAdminUp("port0.10"));
    EXPECT_EQ(calls, vppMock().callsTo("interface_set_state", "port0.10").size());
}

TEST_F(SwitchVppRifTest, rollbackRetainsDeferredAdminOnlyIfOwnedInterfaceDeletionFails)
{
    SwitchVpp::RifCreateState state;
    state.hwif = "port0.10";
    state.parent = "port0";
    state.vlan = 10;
    state.interface_created = true;
    vppMock().interfaces.insert(state.hwif);
    sw->m_sub_if_admin_up_pending.insert(state.hwif);
    vppMock().deleteResult = -1;
    EXPECT_EQ(SAI_STATUS_FAILURE, sw->rollbackRifCreate(state));
    EXPECT_EQ(1u, sw->m_sub_if_admin_up_pending.count(state.hwif));
    vppMock().deleteResult = 0;
    EXPECT_EQ(SAI_STATUS_SUCCESS, sw->rollbackRifCreate(state));
    EXPECT_TRUE(sw->m_sub_if_admin_up_pending.empty());
}

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
    EXPECT_EQ(0u, vppMock().statsCalls);
}

TEST_F(SwitchVppRifTest, featureQueryErrorsDoNotPersistCreateOrSet)
{
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
    ASSERT_EXIT({
        auto attrs = attributes(SAI_ROUTER_INTERFACE_TYPE_PORT, port);
        const auto rif = allocate(SAI_OBJECT_TYPE_ROUTER_INTERFACE);
        vppMock().featureResult = -1;
        if (create(rif, attrs) != SAI_STATUS_FAILURE ||
                sw->m_objectHash.at(SAI_OBJECT_TYPE_ROUTER_INTERFACE).count(sai_serialize_object_id(rif)) != 0)
        {
            _exit(1);
        }
        const auto action = attrs.back();
        attrs.pop_back();
        if (create(rif, attrs) != SAI_STATUS_SUCCESS ||
                sw->vpp_update_router_interface(rif, 1, &action) != SAI_STATUS_FAILURE ||
                vppMock().loopbackCalls != 0)
        {
            _exit(1);
        }
        const auto& stored = sw->m_objectHash.at(SAI_OBJECT_TYPE_ROUTER_INTERFACE).at(sai_serialize_object_id(rif));
        if (stored.count("SAI_ROUTER_INTERFACE_ATTR_LOOPBACK_PACKET_ACTION") != 0)
        {
            _exit(1);
        }
        vppMock().featureResult = 0;
        if (sw->vpp_update_router_interface(rif, 1, &action) != SAI_STATUS_SUCCESS)
        {
            _exit(1);
        }
        sai_attribute_t value{};
        value.id = SAI_ROUTER_INTERFACE_ATTR_LOOPBACK_PACKET_ACTION;
        _exit(sw->get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, rif, 1, &value) == SAI_STATUS_SUCCESS &&
                value.value.s32 == SAI_PACKET_ACTION_DROP && vppMock().loopbackCalls == 1 ? 0 : 1);
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
        auto& stats = vppMock().interfaceStats[name];
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
            vppMock().kernelVrfId = 100;
            vppMock().interfaceVrfs[name] = 0;
            vppMock().loopbackResult = failSnapshot ? 0 : -1;
            vppMock().statsResult = failSnapshot ? -1 : 0;
            ASSERT_EQ(SAI_STATUS_FAILURE, create(rif, attrs));
            EXPECT_EQ(0u, vppMock().interfaceVrfs.at(name));
            EXPECT_NE(nullptr, sw->m_ifaceRegistry.findByOid(backing));
            EXPECT_TRUE(sw->m_ifaceRegistry.resolveHwIfByRifOid(rif).empty());
            EXPECT_EQ(1u, vppMock().vrfTables.count(100));
            EXPECT_EQ(1u, sw->vrf_objMap.count(nonDefaultVrf));

            vppMock().kernelVrfId = 0;
            vppMock().loopbackResult = vppMock().statsResult = 0;
            attrs[2].value.oid = vrf;
            ASSERT_EQ(SAI_STATUS_SUCCESS, create(rif, attrs));
            EXPECT_EQ(0u, vppMock().interfaceVrfs.at(name));
            ASSERT_EQ(SAI_STATUS_SUCCESS, sw->removeRouterif(rif));
        }
    }
}

TEST_F(SwitchVppRifTest, rollbackRestoresActualPreviousVrfAndDefaultCreateBindsTableZero)
{
    const auto rif = allocate(SAI_OBJECT_TYPE_ROUTER_INTERFACE);
    auto attrs = attributes(SAI_ROUTER_INTERFACE_TYPE_PORT, port);
    attrs[2].value.oid = allocate(SAI_OBJECT_TYPE_VIRTUAL_ROUTER);
    vppMock().interfaceVrfs["port0"] = 42;
    vppMock().vrfTables.insert(42);
    vppMock().kernelVrfId = 100;
    vppMock().statsResult = -1;
    ASSERT_EQ(SAI_STATUS_FAILURE, create(rif, attrs));
    EXPECT_EQ(42u, vppMock().interfaceVrfs.at("port0"));
    EXPECT_EQ(1u, vppMock().vrfTables.count(42));
    EXPECT_EQ(1u, vppMock().vrfTables.count(100));

    vppMock().kernelVrfId = 0;
    vppMock().statsResult = 0;
    attrs[2].value.oid = vrf;
    ASSERT_EQ(SAI_STATUS_SUCCESS, create(rif, attrs));
    EXPECT_EQ(0u, vppMock().interfaceVrfs.at("port0"));
}

TEST_F(SwitchVppRifTest, vrfLookupAndProgrammingErrorsAbortBeforeActionProgramming)
{
    for (const std::string failure : {"kernel", "get", "set"})
    {
        SCOPED_TRACE(failure);
        const auto rif = allocate(SAI_OBJECT_TYPE_ROUTER_INTERFACE);
        auto attrs = attributes(SAI_ROUTER_INTERFACE_TYPE_PORT, port);
        attrs[2].value.oid = allocate(SAI_OBJECT_TYPE_VIRTUAL_ROUTER);
        vppMock().kernelVrfId = 100;
        vppMock().interfaceVrfs["port0"] = 42;
        vppMock().kernelVrfResult = failure == "kernel" ? -1 : 0;
        vppMock().getVrfResult = failure == "get" ? -1 : 0;
        vppMock().setVrfResult = failure == "set" ? -1 : 0;
        vppMock().vrfSetCalls = vppMock().loopbackCalls = 0;
        ASSERT_EQ(SAI_STATUS_FAILURE, create(rif, attrs));
        EXPECT_EQ(42u, vppMock().interfaceVrfs.at("port0"));
        EXPECT_EQ(0u, vppMock().loopbackCalls);
        EXPECT_EQ(failure == "set" ? 1u : 0u, vppMock().vrfSetCalls);
        EXPECT_TRUE(sw->m_ifaceRegistry.resolveHwIfByRifOid(rif).empty());
    }
}

TEST_F(SwitchVppRifTest, loopbackCapabilityMatchesEnabledAndDisabledFeature)
{
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
    for (bool enabled : {false, true})
    {
        ASSERT_EXIT({
            vppMock().loopbackEnabled = enabled;
            sai_attr_capability_t capability{};
            const auto status = sw->queryAttributeCapability(switchId, SAI_OBJECT_TYPE_ROUTER_INTERFACE,
                    SAI_ROUTER_INTERFACE_ATTR_LOOPBACK_PACKET_ACTION, &capability);
            _exit(status == SAI_STATUS_SUCCESS && capability.create_implemented == enabled &&
                    capability.set_implemented == enabled && capability.get_implemented == enabled &&
                    vppMock().queriedFeature == "iface-loopback" && vppMock().featureCalls == 1 ? 0 : 1);
        }, ::testing::ExitedWithCode(0), "");
    }
}

TEST_F(SwitchVppRifTest, loopbackCapabilityQueryFailureIsRetryable)
{
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
    ASSERT_EXIT({
        sai_attr_capability_t capability{};
        vppMock().featureResult = -1;
        if (sw->queryAttributeCapability(switchId, SAI_OBJECT_TYPE_ROUTER_INTERFACE,
                    SAI_ROUTER_INTERFACE_ATTR_LOOPBACK_PACKET_ACTION, &capability) != SAI_STATUS_FAILURE)
        {
            _exit(1);
        }
        vppMock().featureResult = 0;
        const auto status = sw->queryAttributeCapability(switchId, SAI_OBJECT_TYPE_ROUTER_INTERFACE,
                SAI_ROUTER_INTERFACE_ATTR_LOOPBACK_PACKET_ACTION, &capability);
        _exit(status == SAI_STATUS_SUCCESS && capability.create_implemented &&
                capability.set_implemented && capability.get_implemented && vppMock().featureCalls == 2 ? 0 : 1);
    }, ::testing::ExitedWithCode(0), "");
}

TEST_F(SwitchVppRifTest, unrelatedCapabilitiesDoNotQueryLoopbackFeature)
{
    vppMock().featureResult = -1;
    sai_attr_capability_t capability{};
    ASSERT_EQ(SAI_STATUS_SUCCESS, sw->queryAttributeCapability(switchId, SAI_OBJECT_TYPE_ROUTER_INTERFACE,
            SAI_ROUTER_INTERFACE_ATTR_TYPE, &capability));
    EXPECT_TRUE(capability.create_implemented && capability.set_implemented && capability.get_implemented);
    ASSERT_EQ(SAI_STATUS_SUCCESS, sw->queryAttributeCapability(switchId, SAI_OBJECT_TYPE_PORT,
            SAI_PORT_ATTR_MTU, &capability));
    EXPECT_TRUE(capability.create_implemented && capability.set_implemented && capability.get_implemented);
    EXPECT_EQ(0u, vppMock().featureCalls);
}

TEST_F(SwitchVppRifTest, queryFailureDoesNotAcknowledgeClearOrChangeBaseline)
{
    const auto rif = allocate(SAI_OBJECT_TYPE_ROUTER_INTERFACE);
    vppMock().interfaceStats["port0"].tx = 100;
    ASSERT_EQ(SAI_STATUS_SUCCESS, create(rif, attributes(SAI_ROUTER_INTERFACE_TYPE_PORT, port)));
    const auto baseline = sw->m_rifStatsBaseMap.at(rif);
    const sai_stat_id_t id = SAI_ROUTER_INTERFACE_STAT_OUT_PACKETS;
    uint64_t value = 999;
    vppMock().statsResult = -1;
    ASSERT_EQ(SAI_STATUS_FAILURE, sw->getRifStatsExt(rif, 1, &id, SAI_STATS_MODE_READ_AND_CLEAR, &value));
    EXPECT_EQ(999u, value);
    EXPECT_EQ(baseline, sw->m_rifStatsBaseMap.at(rif));
    vppMock().statsResult = 0;
    vppMock().interfaceStats["port0"].tx = 130;
    ASSERT_EQ(SAI_STATUS_SUCCESS, sw->getRifStatsExt(rif, 1, &id, SAI_STATS_MODE_READ_AND_CLEAR, &value));
    EXPECT_EQ(30u, value);
    ASSERT_EQ(SAI_STATUS_SUCCESS, sw->getRifStatsExt(rif, 1, &id, SAI_STATS_MODE_READ, &value));
    EXPECT_EQ(0u, value);
}

TEST_F(SwitchVppRifTest, resetDiscardsOnlyTheAffectedBaseline)
{
    const auto rif = allocate(SAI_OBJECT_TYPE_ROUTER_INTERFACE);
    vppMock().interfaceStats["port0"].tx = 100;
    vppMock().interfaceStats["port0"].tx_error = 30;
    ASSERT_EQ(SAI_STATUS_SUCCESS, create(rif, attributes(SAI_ROUTER_INTERFACE_TYPE_PORT, port)));
    const sai_stat_id_t ids[] = {
        SAI_ROUTER_INTERFACE_STAT_OUT_PACKETS, SAI_ROUTER_INTERFACE_STAT_OUT_ERROR_PACKETS};
    uint64_t values[2]{};
    vppMock().interfaceStats["port0"].tx = 5;
    vppMock().interfaceStats["port0"].tx_error = 35;
    ASSERT_EQ(SAI_STATUS_SUCCESS, sw->getRifStatsExt(rif, 2, ids, SAI_STATS_MODE_READ, values));
    EXPECT_EQ(5u, values[0]);
    EXPECT_EQ(5u, values[1]);
    vppMock().interfaceStats["port0"].tx = 101;
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
    EXPECT_EQ(0u, vppMock().statsCalls);
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
    EXPECT_EQ(0u, vppMock().interfaceCreates);
    EXPECT_EQ(0u, vppMock().loopbackCalls);
}

TEST_F(SwitchVppRifTest, failedCreatesUnwindOwnedResourcesAndCanBeRetried)
{
    const auto parentRif = allocate(SAI_OBJECT_TYPE_ROUTER_INTERFACE);
    ASSERT_TRUE(sw->m_ifaceRegistry.setRifOid("port0", parentRif));
    vppMock().actions["port0"] = 1;
    for (auto type : {SAI_ROUTER_INTERFACE_TYPE_VLAN, SAI_ROUTER_INTERFACE_TYPE_SUB_PORT})
    {
        const std::string name = type == SAI_ROUTER_INTERFACE_TYPE_VLAN ? "bvi100" : "port0.10";
        const auto attrs = attributes(type, type == SAI_ROUTER_INTERFACE_TYPE_VLAN ? vlan : port);
        for (const std::string failure : {"action", "snapshot", "lcp"})
        {
            SCOPED_TRACE(name + " " + failure);
            const auto rif = allocate(SAI_OBJECT_TYPE_ROUTER_INTERFACE);
            vppMock().loopbackResult = failure == "action" ? -1 : 0;
            vppMock().statsResult = failure == "snapshot" ? -1 : 0;
            vppMock().lcpResult = failure == "lcp" ? -1 : 0;
            ASSERT_EQ(SAI_STATUS_FAILURE, create(rif, attrs));
            EXPECT_EQ(0u, vppMock().interfaces.count(name));
            EXPECT_EQ(0u, vppMock().lcpPairs.count(name));
            EXPECT_EQ(0u, vppMock().bridgePorts.count(name));
            EXPECT_EQ(nullptr, sw->m_ifaceRegistry.findByHwif(name));
            EXPECT_TRUE(sw->m_ifaceRegistry.resolveHwIfByRifOid(rif).empty());
            EXPECT_EQ(0u, sw->m_rifStatsBaseMap.count(rif));
            EXPECT_EQ(0u, sw->m_objectHash.at(SAI_OBJECT_TYPE_ROUTER_INTERFACE).count(sai_serialize_object_id(rif)));
            EXPECT_EQ("port0", sw->m_ifaceRegistry.resolveHwIfByRifOid(parentRif));
            EXPECT_EQ(1, vppMock().actions.at("port0"));
            EXPECT_EQ(1u, sw->m_objectHash.at(SAI_OBJECT_TYPE_VLAN).count(sai_serialize_object_id(vlan)));
            EXPECT_EQ(1u, sw->vrf_objMap.count(vrf));

            vppMock().loopbackResult = vppMock().statsResult = vppMock().lcpResult = 0;
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
        vppMock().interfaces.insert(name);
        vppMock().lcpPairs.insert(name);
        const auto rif = allocate(SAI_OBJECT_TYPE_ROUTER_INTERFACE);
        EXPECT_EQ(SAI_STATUS_FAILURE, create(rif, attributes(
                type, type == SAI_ROUTER_INTERFACE_TYPE_VLAN ? vlan : port)));
        EXPECT_EQ(1u, vppMock().interfaces.count(name));
        EXPECT_EQ(1u, vppMock().lcpPairs.count(name));
    }
}

TEST_F(SwitchVppRifTest, failedPortSnapshotRestoresActionWithoutDeletingPort)
{
    const auto rif = allocate(SAI_OBJECT_TYPE_ROUTER_INTERFACE);
    vppMock().statsResult = -1;
    ASSERT_EQ(SAI_STATUS_FAILURE, create(rif, attributes(SAI_ROUTER_INTERFACE_TYPE_PORT, port)));
    EXPECT_EQ(0, vppMock().actions.at("port0"));
    EXPECT_NE(nullptr, sw->m_ifaceRegistry.findByOid(port));
    EXPECT_TRUE(sw->m_ifaceRegistry.resolveHwIfByRifOid(rif).empty());
    EXPECT_EQ(0u, sw->m_rifStatsBaseMap.count(rif));
}

/*
 * SAI_ROUTER_INTERFACE_ATTR_ADMIN_V4_STATE and ADMIN_V6_STATE on a VLAN RIF
 * turn VPP's ip4-not-enabled and ip6-not-enabled drop features on and off on
 * the VLAN's bridge interface.
 */
namespace
{
    const std::string IP4_DROP = "ip4-unicast/ip4-not-enabled";
    const std::string IP6_DROP = "ip6-unicast/ip6-not-enabled";

    sai_attribute_t adminState(sai_attr_id_t id, bool up)
    {
        SWSS_LOG_ENTER();
        sai_attribute_t attr{};
        attr.id = id;
        attr.value.booldata = up;
        return attr;
    }
}

TEST_F(SwitchVppRifTest, vlanAdminStateTogglesTheDropOncePerChange)
{
    const auto rif = allocate(SAI_OBJECT_TYPE_ROUTER_INTERFACE);
    ASSERT_EQ(SAI_STATUS_SUCCESS, create(rif, attributes(SAI_ROUTER_INTERFACE_TYPE_VLAN, vlan)));
    EXPECT_TRUE(vppMock().callsTo(IP4_DROP, "bvi100").empty());

    // VPP keeps a copy per enable, so a repeated down must not add another
    for (bool up : {false, false, true, true})
    {
        auto attr = adminState(SAI_ROUTER_INTERFACE_ATTR_ADMIN_V4_STATE, up);
        ASSERT_EQ(SAI_STATUS_SUCCESS, sw->vpp_update_router_interface(rif, 1, &attr));
    }
    const auto calls = vppMock().callsTo(IP4_DROP, "bvi100");
    ASSERT_EQ(2u, calls.size());
    EXPECT_TRUE(calls[0].flag);
    EXPECT_FALSE(calls[1].flag);
    EXPECT_TRUE(vppMock().callsTo(IP6_DROP, "bvi100").empty());
}

TEST_F(SwitchVppRifTest, vlanAdminV6DownLeavesV4Alone)
{
    const auto rif = allocate(SAI_OBJECT_TYPE_ROUTER_INTERFACE);
    ASSERT_EQ(SAI_STATUS_SUCCESS, create(rif, attributes(SAI_ROUTER_INTERFACE_TYPE_VLAN, vlan)));
    auto attr = adminState(SAI_ROUTER_INTERFACE_ATTR_ADMIN_V6_STATE, false);
    ASSERT_EQ(SAI_STATUS_SUCCESS, sw->vpp_update_router_interface(rif, 1, &attr));
    const auto calls = vppMock().callsTo(IP6_DROP, "bvi100");
    ASSERT_EQ(1u, calls.size());
    EXPECT_TRUE(calls[0].flag);
    EXPECT_TRUE(vppMock().callsTo(IP4_DROP, "bvi100").empty());
}

/*
 * The admin states are create-and-set attributes: one passed on create was
 * stored without being applied, so the interface forwarded, and the later up then
 * removed a drop it never added.
 */
TEST_F(SwitchVppRifTest, vlanCreatedAdminDownDropsFromTheStart)
{
    const auto rif = allocate(SAI_OBJECT_TYPE_ROUTER_INTERFACE);
    auto attrs = attributes(SAI_ROUTER_INTERFACE_TYPE_VLAN, vlan);
    attrs.push_back(adminState(SAI_ROUTER_INTERFACE_ATTR_ADMIN_V4_STATE, false));
    attrs.push_back(adminState(SAI_ROUTER_INTERFACE_ATTR_ADMIN_V6_STATE, true));
    ASSERT_EQ(SAI_STATUS_SUCCESS, create(rif, attrs));
    auto calls = vppMock().callsTo(IP4_DROP, "bvi100");
    ASSERT_EQ(1u, calls.size());
    EXPECT_TRUE(calls[0].flag);
    EXPECT_TRUE(vppMock().callsTo(IP6_DROP, "bvi100").empty());

    auto attr = adminState(SAI_ROUTER_INTERFACE_ATTR_ADMIN_V4_STATE, true);
    ASSERT_EQ(SAI_STATUS_SUCCESS, sw->vpp_update_router_interface(rif, 1, &attr));
    calls = vppMock().callsTo(IP4_DROP, "bvi100");
    ASSERT_EQ(2u, calls.size());
    EXPECT_FALSE(calls[1].flag);
}

TEST_F(SwitchVppRifTest, vlanCreateFailsAndUnwindsIfTheDropCannotBeSet)
{
    const auto rif = allocate(SAI_OBJECT_TYPE_ROUTER_INTERFACE);
    auto attrs = attributes(SAI_ROUTER_INTERFACE_TYPE_VLAN, vlan);
    attrs.push_back(adminState(SAI_ROUTER_INTERFACE_ATTR_ADMIN_V4_STATE, false));
    vppMock().featureEnableResult = -1;
    ASSERT_EQ(SAI_STATUS_FAILURE, create(rif, attrs));
    EXPECT_EQ(0u, vppMock().interfaces.count("bvi100"));
    EXPECT_EQ(nullptr, sw->m_ifaceRegistry.findByHwif("bvi100"));
    EXPECT_EQ(0u, sw->m_objectHash.at(SAI_OBJECT_TYPE_ROUTER_INTERFACE).count(sai_serialize_object_id(rif)));

    vppMock().featureEnableResult = 0;
    ASSERT_EQ(SAI_STATUS_SUCCESS, create(rif, attrs));
}
