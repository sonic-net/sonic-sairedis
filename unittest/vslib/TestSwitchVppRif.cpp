#include "MockVppXlate.h"
#include "SwitchVppTestHarness.h"
#include "vpp/vppxlate/SaiIntfStats.h"
#include "ContextConfigContainer.h"
#include "meta/DummySaiInterface.h"
#include "meta/Meta.h"
#include "meta/sai_serialize.h"
#include "saivs.h"

#include <gtest/gtest.h>

#include <algorithm>
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
        if (SwitchVpp::vpp_apply_loopback_action("Ethernet0", SAI_PACKET_ACTION_DROP) != SAI_STATUS_NOT_SUPPORTED)
        {
            _exit(1);
        }
        auto status = SwitchVpp::vpp_apply_loopback_action("Ethernet0", SAI_PACKET_ACTION_FORWARD);
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

TEST_F(SwitchVppRifTest, subPortCreateUsesSonicNameForVrfLookup)
{
    for (const std::string osif : {"Ethernet0.10", "Po2.10", "PortChannel2.10"})
    {
        SCOPED_TRACE(osif);
        auto backing = osif == "Ethernet0.10" ? port : lag;
        const std::string hwif = backing == port ? "port0.10" : "BondEthernet2.10";
        const std::string tap = backing == port ? "tap0.10" : "be2.10";
        vppMock().kernelInterfaces = {osif};
        vppMock().kernelCommands.clear();
        vppMock().kernelVrfId = 100;
        auto rif = allocate(SAI_OBJECT_TYPE_ROUTER_INTERFACE);
        ASSERT_EQ(SAI_STATUS_SUCCESS, create(rif, attributes(SAI_ROUTER_INTERFACE_TYPE_SUB_PORT, backing)));
        EXPECT_NE(vppMock().kernelCommands.end(), std::find(vppMock().kernelCommands.begin(),
                vppMock().kernelCommands.end(), "/sbin/ip link show dev " + osif));
        EXPECT_EQ(100u, vppMock().interfaceVrfs[hwif]);
        EXPECT_EQ(hwif, sw->m_ifaceRegistry.resolveHwIfByRifOid(rif));
        EXPECT_EQ(osif, sw->m_ifaceRegistry.resolveOsIfByHwif(hwif));
        ASSERT_NE(nullptr, sw->m_ifaceRegistry.findByHwif(hwif));
        EXPECT_EQ(sw->m_ifaceRegistry.findByHwif(hwif), sw->m_ifaceRegistry.findByOsIf(osif));
        EXPECT_EQ(tap, sw->m_ifaceRegistry.findByHwif(hwif)->getTapName());
        ASSERT_EQ(SAI_STATUS_SUCCESS, sw->removeRouterif(rif));
    }

    vppMock().kernelInterfaces = {"Ethernet0.10"};
    auto rif = allocate(SAI_OBJECT_TYPE_ROUTER_INTERFACE);
    EXPECT_EQ(SAI_STATUS_FAILURE, create(rif, attributes(SAI_ROUTER_INTERFACE_TYPE_SUB_PORT, lag)));
    EXPECT_EQ(nullptr, sw->m_ifaceRegistry.findSubIf("BondEthernet2", 10));
    EXPECT_TRUE(sw->m_ifaceRegistry.resolveHwIfByRifOid(rif).empty());
}

TEST_F(SwitchVppRifTest, lagDiscoveryIgnoresSubinterfaceParentSuffix)
{
    ASSERT_NE(nullptr, sw->m_ifaceRegistry.addLag(54, 54, allocate(SAI_OBJECT_TYPE_LAG)));
    vppMock().kernelLinkNames = "PortChannel2\nPortChannel54\nPo54.54@PortChannel54\nPortChannel57\n";
    EXPECT_EQ(57u, sw->find_new_bond_id());
}

TEST_F(SwitchVppRifTest, lagDiscoveryRejectsMalformedAndOutOfRangeIds)
{
    for (const std::string name : {"PortChannel", "PortChannel-1", "PortChannel+1",
            "PortChannel 1", "PortChannel54.54", "PortChannel57@Ethernet0",
            "PortChannel2147483648x", "PortChannel4294967295", "PortChannel4294967296",
            "PortChannel999999999999999999999999999999999"})
    {
        SCOPED_TRACE(name);
        vppMock().kernelLinkNames = name + "\nPortChannel2\nPortChannel57\n";
        EXPECT_EQ(57u, sw->find_new_bond_id());
    }
}

TEST_F(SwitchVppRifTest, lagDiscoveryReportsMissingCandidateAndCommandFailure)
{
    vppMock().kernelLinkNames = "PortChannel2\n";
    EXPECT_EQ(~0u, sw->find_new_bond_id());
    vppMock().kernelLinkNames.clear();
    EXPECT_EQ(~0u, sw->find_new_bond_id());
    vppMock().kernelLinkNames = "PortChannel57\n";
    vppMock().kernelLinkResult = 1;
    EXPECT_EQ(~0u, sw->find_new_bond_id());
}

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

TEST_F(SwitchVppRifTest, disabledLoopbackCreateFallsBackAndSetDoesNotPersist)
{
    struct CreateCase
    {
        sai_packet_action_t action;
        bool omitAction;
    };
    const CreateCase cases[] = {
        {SAI_PACKET_ACTION_DROP, false},
        {SAI_PACKET_ACTION_FORWARD, false},
        {SAI_PACKET_ACTION_FORWARD, true},
    };
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
    ASSERT_EXIT({
        vppMock().loopbackEnabled = false;
        for (auto backing : {port, lag, vlan})
        {
            for (auto type : {SAI_ROUTER_INTERFACE_TYPE_PORT, SAI_ROUTER_INTERFACE_TYPE_SUB_PORT,
                             SAI_ROUTER_INTERFACE_TYPE_VLAN})
            {
                if ((backing == vlan) != (type == SAI_ROUTER_INTERFACE_TYPE_VLAN))
                {
                    continue;
                }
                const auto rif = allocate(SAI_OBJECT_TYPE_ROUTER_INTERFACE);
                const auto sid = sai_serialize_object_id(rif);
                if (create(rif, attributes(type, backing, SAI_PACKET_ACTION_TRAP)) != SAI_STATUS_NOT_SUPPORTED ||
                        sw->m_objectHash.at(SAI_OBJECT_TYPE_ROUTER_INTERFACE).count(sid) != 0 ||
                        !sw->m_ifaceRegistry.resolveHwIfByRifOid(rif).empty() ||
                        sw->m_rifStatsBaseMap.count(rif) != 0)
                {
                    _exit(1);
                }

                for (const auto& createCase : cases)
                {
                    auto attrs = attributes(type, backing, createCase.action);
                    if (createCase.omitAction)
                    {
                        attrs.erase(attrs.begin() + 4);
                    }
                    if (create(rif, attrs) != SAI_STATUS_SUCCESS ||
                            sw->m_ifaceRegistry.resolveHwIfByRifOid(rif).empty() ||
                            sw->m_rifStatsBaseMap.count(rif) != 1)
                    {
                        _exit(2);
                    }
                    const auto stored = sw->m_objectHash.at(SAI_OBJECT_TYPE_ROUTER_INTERFACE).at(sid);
                    const bool explicitForward = !createCase.omitAction &&
                        createCase.action == SAI_PACKET_ACTION_FORWARD;
                    if (stored.count("SAI_ROUTER_INTERFACE_ATTR_LOOPBACK_PACKET_ACTION") !=
                            static_cast<size_t>(explicitForward))
                    {
                        _exit(3);
                    }
                    sai_attribute_t action{};
                    action.id = SAI_ROUTER_INTERFACE_ATTR_LOOPBACK_PACKET_ACTION;
                    action.value.s32 = SAI_PACKET_ACTION_DROP;
                    if (sw->set(SAI_OBJECT_TYPE_ROUTER_INTERFACE, rif, &action) != SAI_STATUS_NOT_SUPPORTED ||
                            sw->m_objectHash.at(SAI_OBJECT_TYPE_ROUTER_INTERFACE).at(sid) != stored)
                    {
                        _exit(4);
                    }
                    sai_attribute_t requested[2]{};
                    requested[0].id = SAI_ROUTER_INTERFACE_ATTR_LOOPBACK_PACKET_ACTION;
                    requested[0].value.s32 = SAI_PACKET_ACTION_TRAP;
                    requested[1].id = SAI_ROUTER_INTERFACE_ATTR_TYPE;
                    requested[1].value.s32 = -1;
                    if (sw->get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, rif, 2, requested) != SAI_STATUS_SUCCESS ||
                            requested[0].value.s32 != SAI_PACKET_ACTION_FORWARD ||
                            requested[1].value.s32 != type)
                    {
                        _exit(5);
                    }
                    action.value.s32 = SAI_PACKET_ACTION_FORWARD;
                    if (sw->set(SAI_OBJECT_TYPE_ROUTER_INTERFACE, rif, &action) != SAI_STATUS_SUCCESS ||
                            sw->removeRouterif(rif) != SAI_STATUS_SUCCESS)
                    {
                        _exit(6);
                    }
                }
            }
        }
        _exit(vppMock().loopbackCalls == 0 ? 0 : 7);
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

TEST_F(SwitchVppRifTest, omittedLoopbackActionGetReturnsDefaultAndContinues)
{
    const auto rif = allocate(SAI_OBJECT_TYPE_ROUTER_INTERFACE);
    sai_attribute_t requested[2]{};
    requested[0].id = SAI_ROUTER_INTERFACE_ATTR_LOOPBACK_PACKET_ACTION;
    requested[0].value.s32 = SAI_PACKET_ACTION_TRAP;
    requested[1].id = SAI_ROUTER_INTERFACE_ATTR_TYPE;
    requested[1].value.s32 = -1;
    EXPECT_EQ(SAI_STATUS_ITEM_NOT_FOUND, sw->get(
            SAI_OBJECT_TYPE_ROUTER_INTERFACE, rif, 2, requested));

    auto attrs = attributes(SAI_ROUTER_INTERFACE_TYPE_PORT, port);
    ASSERT_EQ(SAI_ROUTER_INTERFACE_ATTR_LOOPBACK_PACKET_ACTION, attrs.back().id);
    attrs.pop_back();
    ASSERT_EQ(SAI_STATUS_SUCCESS, create(rif, attrs));
    EXPECT_EQ(SAI_STATUS_SUCCESS, sw->get(
            SAI_OBJECT_TYPE_ROUTER_INTERFACE, rif, 2, requested));
    EXPECT_EQ(SAI_PACKET_ACTION_FORWARD, requested[0].value.s32);
    EXPECT_EQ(SAI_ROUTER_INTERFACE_TYPE_PORT, requested[1].value.s32);

    for (auto action : {SAI_PACKET_ACTION_DROP, SAI_PACKET_ACTION_FORWARD})
    {
        SCOPED_TRACE(action);
        sai_attribute_t attr{};
        attr.id = SAI_ROUTER_INTERFACE_ATTR_LOOPBACK_PACKET_ACTION;
        attr.value.s32 = action;
        ASSERT_EQ(SAI_STATUS_SUCCESS, sw->set(SAI_OBJECT_TYPE_ROUTER_INTERFACE, rif, &attr));
        requested[0].value.s32 = SAI_PACKET_ACTION_TRAP;
        requested[1].value.s32 = -1;
        ASSERT_EQ(SAI_STATUS_SUCCESS, sw->get(
                SAI_OBJECT_TYPE_ROUTER_INTERFACE, rif, 2, requested));
        EXPECT_EQ(action, requested[0].value.s32);
        EXPECT_EQ(SAI_ROUTER_INTERFACE_TYPE_PORT, requested[1].value.s32);
    }

    sai_attribute_t mtu{};
    mtu.id = SAI_ROUTER_INTERFACE_ATTR_MTU;
    EXPECT_EQ(SAI_STATUS_ITEM_NOT_FOUND, sw->get(
            SAI_OBJECT_TYPE_ROUTER_INTERFACE, rif, 1, &mtu));
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
 * VPP builds an interface's IPv6 link-local inside ip6_link_enable(), which
 * only runs when something takes an IPv6 lock on it, so a RIF has to take one
 * of its own instead of waiting for a global address to arrive.
 */
TEST_F(SwitchVppRifTest, rifCreateEnablesIpv6AndRemoveReleasesIt)
{
    for (auto backing : {port, lag})
    {
        const std::string name = backing == port ? "port0" : "BondEthernet2";
        SCOPED_TRACE(name);
        const auto rif = allocate(SAI_OBJECT_TYPE_ROUTER_INTERFACE);

        ASSERT_EQ(SAI_STATUS_SUCCESS, create(rif, attributes(SAI_ROUTER_INTERFACE_TYPE_PORT, backing)));
        ASSERT_EQ(1u, vppMock().callsTo("sw_interface_ip6_enable_disable", name).size());
        EXPECT_TRUE(vppMock().callsTo("sw_interface_ip6_enable_disable", name)[0].flag);
        EXPECT_EQ(1, vppMock().ip6Locks.at(name));

        // the port or bond outlives the RIF, so the lock has to be given back
        ASSERT_EQ(SAI_STATUS_SUCCESS, sw->removeRouterif(rif));
        ASSERT_EQ(2u, vppMock().callsTo("sw_interface_ip6_enable_disable", name).size());
        EXPECT_FALSE(vppMock().callsTo("sw_interface_ip6_enable_disable", name)[1].flag);
        EXPECT_EQ(0, vppMock().ip6Locks.at(name));
    }
}

// An interface this RIF created is deleted on remove, which drops its locks.
TEST_F(SwitchVppRifTest, ownedInterfaceKeepsIpv6LockUntilItIsDeleted)
{
    for (auto type : {SAI_ROUTER_INTERFACE_TYPE_VLAN, SAI_ROUTER_INTERFACE_TYPE_SUB_PORT})
    {
        const bool bvi = type == SAI_ROUTER_INTERFACE_TYPE_VLAN;
        const std::string name = bvi ? "bvi100" : "port0.10";
        SCOPED_TRACE(name);
        const auto rif = allocate(SAI_OBJECT_TYPE_ROUTER_INTERFACE);

        ASSERT_EQ(SAI_STATUS_SUCCESS, create(rif, attributes(type, bvi ? vlan : port)));
        ASSERT_EQ(1u, vppMock().callsTo("sw_interface_ip6_enable_disable", name).size());
        EXPECT_TRUE(vppMock().callsTo("sw_interface_ip6_enable_disable", name)[0].flag);

        ASSERT_EQ(SAI_STATUS_SUCCESS, sw->removeRouterif(rif));
        EXPECT_EQ(1u, vppMock().callsTo("sw_interface_ip6_enable_disable", name).size());
        EXPECT_EQ(0u, vppMock().interfaces.count(name));
    }
}

TEST_F(SwitchVppRifTest, ipv6AdminDownRifTakesNoLock)
{
    auto attrs = attributes(SAI_ROUTER_INTERFACE_TYPE_PORT, port);
    sai_attribute_t adminV6;
    memset(&adminV6, 0, sizeof(adminV6));
    adminV6.id = SAI_ROUTER_INTERFACE_ATTR_ADMIN_V6_STATE;
    adminV6.value.booldata = false;
    attrs.push_back(adminV6);

    const auto rif = allocate(SAI_OBJECT_TYPE_ROUTER_INTERFACE);
    ASSERT_EQ(SAI_STATUS_SUCCESS, create(rif, attrs));
    EXPECT_TRUE(vppMock().callsTo("sw_interface_ip6_enable_disable", "port0").empty());

    // and the remove must not release a lock it never took
    ASSERT_EQ(SAI_STATUS_SUCCESS, sw->removeRouterif(rif));
    EXPECT_TRUE(vppMock().callsTo("sw_interface_ip6_enable_disable", "port0").empty());
}

/*
 * create_bond_interface() lets VPP pick the bond MAC, and the bond only
 * inherits a member's MAC once one is enslaved, which can happen after the RIF
 * is created. ip6_link_enable() derives the link-local once and never
 * recomputes it, so the RIF MAC has to reach the bond first for the link-local
 * to match the kernel's PortChannel.
 */
TEST_F(SwitchVppRifTest, lagRifProgramsBondMacBeforeEnablingIpv6)
{
    const auto rif = allocate(SAI_OBJECT_TYPE_ROUTER_INTERFACE);
    ASSERT_EQ(SAI_STATUS_SUCCESS, create(rif, attributes(SAI_ROUTER_INTERFACE_TYPE_PORT, lag)));

    ASSERT_EQ(1u, vppMock().callsTo("sw_interface_set_mac", "BondEthernet2").size());
    EXPECT_EQ("00:00:00:00:00:01", vppMock().macs.at("BondEthernet2"));

    const int mac = saivs_test::vppCallIndex("sw_interface_set_mac", "BondEthernet2");
    const int ip6 = saivs_test::vppCallIndex("sw_interface_ip6_enable_disable", "BondEthernet2");
    ASSERT_NE(-1, mac);
    ASSERT_NE(-1, ip6);
    EXPECT_LT(mac, ip6);

    // a physical port already had its MAC programmed when its hostif was made
    const auto portRif = allocate(SAI_OBJECT_TYPE_ROUTER_INTERFACE);
    ASSERT_EQ(SAI_STATUS_SUCCESS, create(portRif, attributes(SAI_ROUTER_INTERFACE_TYPE_PORT, port)));
    EXPECT_TRUE(vppMock().callsTo("sw_interface_set_mac", "port0").empty());
}

TEST_F(SwitchVppRifTest, ipv6EnableToleratesAlreadyEnabledButNotOtherErrors)
{
    // VNET_API_ERROR_VALUE_EXIST and _IP6_NOT_ENABLED still moved the lock
    // count, so they are the no-op they look like
    for (int result : {-81, -62})
    {
        SCOPED_TRACE(result);
        const auto rif = allocate(SAI_OBJECT_TYPE_ROUTER_INTERFACE);
        vppMock().ip6EnableResult = result;
        ASSERT_EQ(SAI_STATUS_SUCCESS, create(rif, attributes(SAI_ROUTER_INTERFACE_TYPE_PORT, port)));
        EXPECT_EQ(1u, vppMock().callsTo("sw_interface_ip6_enable_disable", "port0").size());

        vppMock().ip6EnableResult = 0;
        ASSERT_EQ(SAI_STATUS_SUCCESS, sw->removeRouterif(rif));
        vppMock().calls.clear();
    }

    // anything else fails the create, which must leave no RIF behind
    const auto rif = allocate(SAI_OBJECT_TYPE_ROUTER_INTERFACE);
    vppMock().ip6EnableResult = -1;
    ASSERT_EQ(SAI_STATUS_FAILURE, create(rif, attributes(SAI_ROUTER_INTERFACE_TYPE_PORT, port)));
    EXPECT_TRUE(sw->m_ifaceRegistry.resolveHwIfByRifOid(rif).empty());
    EXPECT_EQ(0, vppMock().ip6Locks["port0"]);

    vppMock().ip6EnableResult = 0;
    ASSERT_EQ(SAI_STATUS_SUCCESS, create(rif, attributes(SAI_ROUTER_INTERFACE_TYPE_PORT, port)));
    EXPECT_EQ(1, vppMock().ip6Locks.at("port0"));
}

// A create that fails after the enable has to give the lock back itself.
TEST_F(SwitchVppRifTest, createRollbackReleasesIpv6Lock)
{
    SwitchVpp::RifCreateState state;
    state.hwif = "port0";
    state.ip6_enabled = true;
    vppMock().ip6Locks["port0"] = 1;

    ASSERT_EQ(SAI_STATUS_SUCCESS, sw->rollbackRifCreate(state));
    ASSERT_EQ(1u, vppMock().callsTo("sw_interface_ip6_enable_disable", "port0").size());
    EXPECT_FALSE(vppMock().callsTo("sw_interface_ip6_enable_disable", "port0")[0].flag);
    EXPECT_EQ(0, vppMock().ip6Locks.at("port0"));

    // nothing was taken for an IPv6 admin-down RIF, so nothing is given back
    vppMock().calls.clear();
    SwitchVpp::RifCreateState untouched;
    untouched.hwif = "port0";
    ASSERT_EQ(SAI_STATUS_SUCCESS, sw->rollbackRifCreate(untouched));
    EXPECT_TRUE(vppMock().callsTo("sw_interface_ip6_enable_disable", "port0").empty());
}
