#include "ResourceLimiter.h"

#include <gtest/gtest.h>

using namespace saivs;

TEST(ResourceLimiter, ctr)
{
    ResourceLimiter rl(0);
}

TEST(ResourceLimiter, getObjectTypeLimit)
{
    ResourceLimiter rl(0);

    EXPECT_EQ(rl.getObjectTypeLimit(SAI_OBJECT_TYPE_PORT), SIZE_MAX);

    rl.setObjectTypeLimit(SAI_OBJECT_TYPE_PORT, 5);

    EXPECT_EQ(rl.getObjectTypeLimit(SAI_OBJECT_TYPE_PORT), 5);
}

TEST(ResourceLimiter, removeObjectTypeLimit)
{
    ResourceLimiter rl(0);

    EXPECT_EQ(rl.getObjectTypeLimit(SAI_OBJECT_TYPE_PORT), SIZE_MAX);

    rl.setObjectTypeLimit(SAI_OBJECT_TYPE_PORT, 5);

    EXPECT_EQ(rl.getObjectTypeLimit(SAI_OBJECT_TYPE_PORT), 5);

    rl.removeObjectTypeLimit(SAI_OBJECT_TYPE_PORT);

    EXPECT_EQ(rl.getObjectTypeLimit(SAI_OBJECT_TYPE_PORT), SIZE_MAX);
}

TEST(ResourceLimiter, clearLimits)
{
    ResourceLimiter rl(0);

    rl.setObjectTypeLimit(SAI_OBJECT_TYPE_PORT, 5);

    EXPECT_EQ(rl.getObjectTypeLimit(SAI_OBJECT_TYPE_PORT), 5);

    rl.clearLimits();

    EXPECT_EQ(rl.getObjectTypeLimit(SAI_OBJECT_TYPE_PORT), SIZE_MAX);
}
