#include "LaneMap.h"

#include <gtest/gtest.h>

using namespace saivs;

TEST(LaneMap, add)
{
    LaneMap map(0);

    std::vector<uint32_t> bad;

    EXPECT_EQ(map.add("foo", bad), false);

    std::vector<uint32_t> ok{1};

    EXPECT_EQ(map.add("foo", ok), true);
    EXPECT_EQ(map.add("foo", ok), false);

    // lane in use
    EXPECT_EQ(map.add("bar", ok), false);

    std::vector<uint32_t> bad2 {2,2};

    // non unique lanes
    EXPECT_EQ(map.add("bar", bad2), false);
}

TEST(LandMap, remove)
{
    LaneMap map(0);

    std::vector<uint32_t> ok{1,2,3,4};

    EXPECT_EQ(map.add("foo", ok), true);

    EXPECT_EQ(map.remove("bar"), false);
    EXPECT_EQ(map.remove("foo"), true);


}

TEST(LaneMap, hasInterface)
{
    LaneMap map(0);

    std::vector<uint32_t> bad;

    EXPECT_FALSE(map.hasInterface("foo"));
}

TEST(LaneMap, getInterfaceFromLaneNumber)
{
    LaneMap map(0);

    std::vector<uint32_t> ok{1};

    EXPECT_EQ(map.add("foo", ok), true);

    EXPECT_EQ(map.getInterfaceFromLaneNumber(1), "foo");
    EXPECT_EQ(map.getInterfaceFromLaneNumber(2), "");
}
