#include "RedisRemoteSaiInterface.h"
#include "ContextConfigContainer.h"

#include <gtest/gtest.h>

using namespace sairedis;

TEST(RedisRemoteSaiInterface, queryStatsCapabilityNegative)
{
    auto ctx = ContextConfigContainer::loadFromFile("foo");
    auto rec = std::make_shared<Recorder>();

    RedisRemoteSaiInterface sai(ctx->get(0), nullptr, rec);

    EXPECT_EQ(SAI_STATUS_INVALID_PARAMETER,
              sai.queryStatsCapability(0,
                SAI_OBJECT_TYPE_NULL,
                0));
}
