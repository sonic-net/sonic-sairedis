#include "vpp/VppInterfaceRegistry.h"
#include "vpp/VppAclInterfacePolicy.h"

#include <gtest/gtest.h>

using namespace saivs;

TEST(VppAclInheritance, replayPreservesBothDirectionsAndDefaultTail)
{
    VppAclInterfacePolicy policy;
    policy.updateTable(30, true, true);
    policy.updateTable(10, true, true);
    policy.updateTable(30, true, true); // Repeated bind does not reorder a table.
    policy.ingressDefault = true;
    policy.updateTable(20, false, true);
    policy.egressDefault = true;

    const auto plan = policy.bindingPlan();
    ASSERT_EQ(5u, plan.size());
    EXPECT_EQ(30u, plan[0].table);
    EXPECT_EQ(10u, plan[1].table);
    EXPECT_EQ(SAI_NULL_OBJECT_ID, plan[2].table);
    EXPECT_TRUE(plan[0].is_input);
    EXPECT_TRUE(plan[1].is_input);
    EXPECT_TRUE(plan[2].is_input);
    EXPECT_EQ(20u, plan[3].table);
    EXPECT_EQ(SAI_NULL_OBJECT_ID, plan[4].table);
    EXPECT_FALSE(plan[3].is_input);
    EXPECT_FALSE(plan[4].is_input);
}

TEST(VppAclInheritance, removalAndPendingDefaultSurviveReplay)
{
    VppAclInterfacePolicy policy;
    policy.updateTable(30, true, true);
    policy.updateTable(10, true, true);
    policy.updateTable(30, true, false);
    policy.updateTable(30, true, false);
    policy.ingressDefaultPending = true; // Failed permit restore must be retried.

    auto plan = policy.bindingPlan();
    ASSERT_EQ(2u, plan.size());
    EXPECT_EQ(10u, plan[0].table);
    EXPECT_EQ(SAI_NULL_OBJECT_ID, plan[1].table);
    EXPECT_FALSE(policy.empty());
    policy.updateTable(10, true, false);
    EXPECT_FALSE(policy.empty());
    policy.ingressDefaultPending = false;
    EXPECT_TRUE(policy.empty());
    EXPECT_TRUE(policy.bindingPlan().empty());
}

TEST(VppAclInheritance, portPolicyCoversEveryBridgedVlan)
{
    VppInterfaceRegistry registry;
    const sai_object_id_t port_oid = 1;
    auto port = registry.addPhysicalPort("Ethernet0", "Ethernet0", port_oid);
    ASSERT_NE(nullptr, port);
    ASSERT_NE(nullptr, registry.addSubInterface(port, 100, 100));
    ASSERT_NE(nullptr, registry.addSubInterface(port, 200, 200));
    ASSERT_TRUE(registry.setBdId("Ethernet0.100", 100));
    ASSERT_TRUE(registry.setBdId("Ethernet0.200", 200));

    EXPECT_EQ((std::vector<std::string>{"Ethernet0", "Ethernet0.100", "Ethernet0.200"}),
              registry.resolveAclInterfaces(port_oid));
    EXPECT_TRUE(registry.resolveAclInterfaces(99).empty());
}

TEST(VppAclInheritance, lagPolicyCoversTaggedMembersAndExcludesRoutedSubports)
{
    VppInterfaceRegistry registry;
    const sai_object_id_t lag_oid = 2;
    auto lag = registry.addLag(1, 10, lag_oid);
    ASSERT_NE(nullptr, lag);
    ASSERT_NE(nullptr, registry.addSubInterface(lag, 100, 100));
    ASSERT_NE(nullptr, registry.addSubInterface(lag, 200, 200));
    ASSERT_TRUE(registry.setBdId("BondEthernet1.100", 100));
    ASSERT_TRUE(registry.setRifOid("BondEthernet1.200", 3));
    // A stale BD association must not turn a routed child into an ACL target.
    ASSERT_TRUE(registry.setBdId("BondEthernet1.200", 200));

    EXPECT_EQ((std::vector<std::string>{"BondEthernet1", "BondEthernet1.100"}),
              registry.resolveAclInterfaces(lag_oid));
}

TEST(VppAclInheritance, memberLifecycleChangesPolicyTargets)
{
    VppInterfaceRegistry registry;
    const sai_object_id_t port_oid = 1;
    auto port = registry.addPhysicalPort("Ethernet0", "Ethernet0", port_oid);
    ASSERT_NE(nullptr, port);
    EXPECT_EQ((std::vector<std::string>{"Ethernet0"}), registry.resolveAclInterfaces(port_oid));

    ASSERT_NE(nullptr, registry.addSubInterface(port, 100, 100));
    // A child is eligible once it is actually a bridge member.
    EXPECT_EQ((std::vector<std::string>{"Ethernet0"}), registry.resolveAclInterfaces(port_oid));
    ASSERT_TRUE(registry.setBdId("Ethernet0.100", 100));
    EXPECT_EQ((std::vector<std::string>{"Ethernet0", "Ethernet0.100"}),
              registry.resolveAclInterfaces(port_oid));

    ASSERT_TRUE(registry.clearBdId("Ethernet0.100"));
    EXPECT_EQ((std::vector<std::string>{"Ethernet0"}), registry.resolveAclInterfaces(port_oid));
    EXPECT_EQ(1u, registry.remove("Ethernet0.100"));
    ASSERT_NE(nullptr, registry.addSubInterface(port, 100, 100));
    EXPECT_EQ((std::vector<std::string>{"Ethernet0"}), registry.resolveAclInterfaces(port_oid));
    EXPECT_EQ(2u, registry.remove("Ethernet0"));
    EXPECT_TRUE(registry.resolveAclInterfaces(port_oid).empty());
}
