#include "ResourceLimiterParser.h"

#include <gtest/gtest.h>

using namespace saivs;

TEST(ResourceLimiterParser, parseFromFile)
{
    EXPECT_NE(ResourceLimiterParser::parseFromFile(nullptr), nullptr);

    EXPECT_NE(ResourceLimiterParser::parseFromFile("not_existing"), nullptr);

    EXPECT_NE(ResourceLimiterParser::parseFromFile("files/resource_limiter_bad.txt"), nullptr);

    EXPECT_NE(ResourceLimiterParser::parseFromFile("files/resource_limiter_ok.txt"), nullptr);

    auto rlc = ResourceLimiterParser::parseFromFile("files/resource_limiter_ok.txt");

    auto rl = rlc->getResourceLimiter(1);

    EXPECT_NE(rl, nullptr);

    EXPECT_EQ(rl->getObjectTypeLimit(SAI_OBJECT_TYPE_PORT), 8);
}
