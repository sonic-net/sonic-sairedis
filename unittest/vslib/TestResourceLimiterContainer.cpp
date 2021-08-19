#include "ResourceLimiterContainer.h"

#include <gtest/gtest.h>

using namespace saivs;

TEST(ResourceLimiterContainer, insert)
{
    auto rl = std::make_shared<ResourceLimiter>(0);

    ResourceLimiterContainer rlc;

    EXPECT_THROW(rlc.insert(0, nullptr), std::runtime_error);

    rlc.insert(0, rl);

    EXPECT_NE(rlc.getResourceLimiter(0), nullptr);
}

TEST(ResourceLimiterContainer, remove)
{
    auto rl = std::make_shared<ResourceLimiter>(0);

    ResourceLimiterContainer rlc;

    EXPECT_THROW(rlc.insert(0, nullptr), std::runtime_error);

    rlc.insert(0, rl);

    EXPECT_NE(rlc.getResourceLimiter(0), nullptr);

    rlc.remove(0);

    EXPECT_EQ(rlc.getResourceLimiter(0), nullptr);
}

TEST(ResourceLimiterContainer, getResourceLimiter)
{
    auto rl = std::make_shared<ResourceLimiter>(0);

    ResourceLimiterContainer rlc;

    EXPECT_THROW(rlc.insert(0, nullptr), std::runtime_error);

    rlc.insert(0, rl);

    EXPECT_NE(rlc.getResourceLimiter(0), nullptr);

    rlc.clear();

    EXPECT_EQ(rlc.getResourceLimiter(0), nullptr);
}
