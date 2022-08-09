#include "CorePortIndexMap.h"

#include <gtest/gtest.h>

using namespace saivs;

TEST(CorePortIndexMap, add)
{
    CorePortIndexMap cpim(0);

    EXPECT_EQ(cpim.isEmpty(), true);

    std::vector<uint32_t> inv {1};

    EXPECT_EQ(cpim.add("foo", inv), false);

    std::vector<uint32_t> ok {1,2};

    EXPECT_EQ(cpim.add("foo", ok), true);

    EXPECT_EQ(cpim.isEmpty(), false);

    EXPECT_EQ(cpim.add("foo", ok), false);
}

TEST(CorePortIndexMap, remove)
{
    CorePortIndexMap cpim(0);

    std::vector<uint32_t> ok {1,2};

    EXPECT_EQ(cpim.add("foo", ok), true);

    EXPECT_EQ(cpim.remove("var"), false);

    EXPECT_EQ(cpim.remove("foo"), true);

    EXPECT_EQ(cpim.isEmpty(), true);
}

TEST(CorePortIndexMap, hasInterface)
{
    CorePortIndexMap cpim(0);

    std::vector<uint32_t> ok {1,2};

    EXPECT_EQ(cpim.add("foo", ok), true);

    EXPECT_EQ(cpim.hasInterface("var"), false);

    EXPECT_EQ(cpim.hasInterface("foo"), true);
}


TEST(CorePortIndexMap, getCorePortIndexVector)
{
    CorePortIndexMap cpim(0);

    EXPECT_EQ(cpim.getCorePortIndexVector().size(), 0);

    std::vector<uint32_t> ok {1,2};

    EXPECT_EQ(cpim.add("foo", ok), true);

    EXPECT_EQ(cpim.getCorePortIndexVector().size(), 1);
}

TEST(CorePortIndexMap, getInterfaceFromCorePortIndex)
{
    CorePortIndexMap cpim(0);

    std::vector<uint32_t> ok {1,2};
    std::vector<uint32_t> inv {3,4};

    EXPECT_EQ(cpim.add("foo", ok), true);

    EXPECT_EQ(cpim.getInterfaceFromCorePortIndex(ok), "foo");

    EXPECT_EQ(cpim.getInterfaceFromCorePortIndex(inv), "");
}
