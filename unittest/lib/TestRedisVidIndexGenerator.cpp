#include "RedisVidIndexGenerator.h"

#include <gtest/gtest.h>

#include <memory>

using namespace sairedis;

TEST(RedisVidIndexGenerator, reset)
{
    auto db = std::make_shared<swss::DBConnector>("ASIC_DB", 0);

    RedisVidIndexGenerator g(db, "FOO");

    g.reset();
}
