#include "TrafficFilterPipes.h"
#include "MACsecIngressFilter.h"


#include <gtest/gtest.h>

using namespace saivs;

TEST(TrafficFilterPipes, uninstallFilter)
{
    TrafficFilterPipes p;

    EXPECT_FALSE(p.uninstallFilter(nullptr));

    auto filter = std::make_shared<MACsecIngressFilter>("foo");
    auto filter2 = std::make_shared<MACsecIngressFilter>("foo");

    EXPECT_TRUE(p.installFilter(0, filter));
    EXPECT_TRUE(p.installFilter(1, filter2));

    EXPECT_FALSE(p.uninstallFilter(nullptr));

    EXPECT_TRUE(p.uninstallFilter(filter2));
}

TEST(TrafficFilterPipes, installFilter)
{
    auto filter = std::make_shared<MACsecIngressFilter>("foo");

    TrafficFilterPipes p;

    EXPECT_TRUE(p.installFilter(0, filter));

    EXPECT_FALSE(p.installFilter(0, filter));
}

TEST(TrafficFilterPipes, execute)
{
    TrafficFilterPipes p;

    uint8_t buf[2];
    size_t len = 2;

    EXPECT_EQ(p.execute(buf, len), TrafficFilter::CONTINUE);

    EXPECT_TRUE(p.installFilter(0, nullptr));
    EXPECT_TRUE(p.installFilter(1, nullptr));

    EXPECT_EQ(p.execute(buf, len), TrafficFilter::CONTINUE);

    auto filter = std::make_shared<MACsecIngressFilter>("foo");

    EXPECT_TRUE(p.installFilter(0, filter));

    EXPECT_EQ(p.execute(buf, len), TrafficFilter::TERMINATE);
}
