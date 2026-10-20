#include "vpp/SwitchVpp.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>
#include <unistd.h>

using namespace saivs;

namespace
{
    bool loopbackEnabled = false;
    std::string queriedFeature;
    unsigned int loopbackCalls = 0;
    int loopbackAction = -1;
    int loopbackResult = 0;
}

extern "C" int __wrap_vpp_sonic_ext_feature_get(const char *feature, bool *enabled)
{
    queriedFeature = feature;
    *enabled = queriedFeature == "iface-loopback" && loopbackEnabled;
    return 0;
}

extern "C" int __wrap_vpp_iface_loopback_set_action(const char *, int action)
{
    ++loopbackCalls;
    loopbackAction = action;
    return loopbackResult;
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
    ASSERT_EXIT({
        auto status = SwitchVpp::vpp_apply_loopback_action("Ethernet0", SAI_PACKET_ACTION_DROP);
        _exit(status == SAI_STATUS_SUCCESS && loopbackCalls == 0 &&
                queriedFeature == "iface-loopback" ? 0 : 1);
    }, ::testing::ExitedWithCode(0), "");
}

TEST(SwitchVpp, loopbackActionEnabled)
{
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
