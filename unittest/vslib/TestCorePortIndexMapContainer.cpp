#include "CorePortIndexMapContainer.h"

#include <gtest/gtest.h>

#include <memory>

using namespace saivs;

TEST(CorePortIndexMapContainer, remove)
{
    auto cpim = std::make_shared<CorePortIndexMap>(0);

    CorePortIndexMapContainer cpimc;

    cpimc.insert(cpim);

    EXPECT_EQ(cpimc.size(), 1);

    cpimc.remove(1);

    EXPECT_EQ(cpimc.size(), 1);

    cpimc.remove(0);

    EXPECT_EQ(cpimc.size(), 0);
}

TEST(CorePortIndexMapContainer, getCorePortIndexMap)
{
    auto cpim = std::make_shared<CorePortIndexMap>(0);

    CorePortIndexMapContainer cpimc;

    cpimc.insert(cpim);

    EXPECT_EQ(cpimc.getCorePortIndexMap(1), nullptr);

    EXPECT_NE(cpimc.getCorePortIndexMap(0), nullptr);
}

TEST(CorePortIndexMapContainer, clear)
{
    auto cpim = std::make_shared<CorePortIndexMap>(0);

    CorePortIndexMapContainer cpimc;

    cpimc.insert(cpim);

    EXPECT_EQ(cpimc.size(), 1);

    cpimc.clear();

    EXPECT_EQ(cpimc.size(), 0);
}

TEST(CorePortIndexMapContainer, hasCorePortIndexMap)
{
    auto cpim = std::make_shared<CorePortIndexMap>(0);

    CorePortIndexMapContainer cpimc;

    cpimc.insert(cpim);

    EXPECT_EQ(cpimc.hasCorePortIndexMap(0), true);

    EXPECT_EQ(cpimc.hasCorePortIndexMap(1), false);
}

TEST(CorePortIndexMapContainer, removeEmptyCorePortIndexMaps)
{
    auto cpime = std::make_shared<CorePortIndexMap>(0);

    CorePortIndexMapContainer cpimc;

    cpimc.insert(cpime);

    auto cpim = std::make_shared<CorePortIndexMap>(1);

    std::vector<uint32_t> ok {1,0};

    EXPECT_EQ(cpim->add("foo", ok), true);

    EXPECT_EQ(cpim->isEmpty(), false);

    cpimc.insert(cpim);

    EXPECT_EQ(cpimc.size(), 2);

    cpimc.removeEmptyCorePortIndexMaps();

    EXPECT_EQ(cpimc.size(), 1);
}
