#include "LaneMapContainer.h"

#include <gtest/gtest.h>

#include <memory>

using namespace saivs;

TEST(LaneMapContainer, insert)
{
    auto map = std::make_shared<LaneMap>(0);

    std::vector<uint32_t> ok{1};

    EXPECT_EQ(map->add("foo", ok), true);

    LaneMapContainer lmc;

    EXPECT_EQ(lmc.insert(map), true);

    EXPECT_EQ(lmc.size(), 1);

    EXPECT_EQ(lmc.insert(map), false);
}

TEST(LaneMapContainer, remove)
{
    auto map = std::make_shared<LaneMap>(0);

    std::vector<uint32_t> ok{1};

    EXPECT_EQ(map->add("foo", ok), true);

    LaneMapContainer lmc;

    lmc.insert(map);

    EXPECT_EQ(lmc.size(), 1);

    EXPECT_TRUE(lmc.remove(0));

    EXPECT_EQ(lmc.size(), 0);

    EXPECT_FALSE(lmc.remove(0));
}

TEST(LaneMapContainer, getLaneMap)
{
    auto map = std::make_shared<LaneMap>(0);

    std::vector<uint32_t> ok{1};

    EXPECT_EQ(map->add("foo", ok), true);

    LaneMapContainer lmc;

    EXPECT_EQ(lmc.insert(map), true);

    EXPECT_NE(lmc.getLaneMap(0), nullptr);

    EXPECT_EQ(lmc.getLaneMap(1), nullptr);
}

TEST(LaneMapContainer, clear)
{
    auto map = std::make_shared<LaneMap>(0);

    std::vector<uint32_t> ok{1};

    EXPECT_EQ(map->add("foo", ok), true);

    LaneMapContainer lmc;

    EXPECT_EQ(lmc.insert(map), true);

    EXPECT_EQ(lmc.size(), 1);

    lmc.clear();

    EXPECT_EQ(lmc.size(), 0);
}

TEST(LaneMapContainer, hasLaneMap)
{
    LaneMapContainer lmc;

    EXPECT_FALSE(lmc.hasLaneMap(0));
}

TEST(LaneMapContainer, removeEmptyLaneMaps)
{
    LaneMapContainer lmc;

    EXPECT_FALSE(lmc.hasLaneMap(0));

    auto map1 = std::make_shared<LaneMap>(0);
    auto map2 = std::make_shared<LaneMap>(1);

    std::vector<uint32_t> ok{1};

    EXPECT_EQ(map1->add("foo", ok), true);

    EXPECT_TRUE(lmc.insert(map1));

    EXPECT_TRUE(lmc.insert(map2));

    EXPECT_EQ(lmc.size(), 2);

    lmc.removeEmptyLaneMaps();

    EXPECT_EQ(lmc.size(), 1);
}
