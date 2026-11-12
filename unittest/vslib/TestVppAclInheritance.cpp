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
