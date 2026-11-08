#include "SwitchContainer.h"

#include <gtest/gtest.h>

using namespace saivs;

TEST(SwitchContainer, insert)
{
    SwitchContainer sc;

    EXPECT_THROW(sc.insert(nullptr), std::runtime_error);

    auto s = std::make_shared<Switch>(0x2100000000);

    sc.insert(s);

    EXPECT_THROW(sc.insert(s), std::runtime_error);
}

TEST(SwitchContainer, removeSwitch)
{
    SwitchContainer sc;

    EXPECT_THROW(sc.removeSwitch(0), std::runtime_error);

    auto s = std::make_shared<Switch>(0x2100000000);

    sc.insert(s);

    sc.removeSwitch(0x2100000000);

    s = std::make_shared<Switch>(0x2100000000);

    sc.insert(s);

    EXPECT_THROW(sc.removeSwitch(nullptr), std::runtime_error);

    EXPECT_THROW(sc.removeSwitch(1), std::runtime_error);

    sc.removeSwitch(s);
}

TEST(SwitchContainer, clear)
{
    SwitchContainer sc;

    sc.clear();
}

TEST(SwitchContainer, contains)
{
    SwitchContainer sc;

    auto s = std::make_shared<Switch>(0x2100000000);

    sc.insert(s);

    EXPECT_TRUE(sc.contains(0x2100000000));

    EXPECT_FALSE(sc.contains(1));
}

TEST(SwitchContainer, getSwitch)
{
    SwitchContainer sc;

    auto s = std::make_shared<Switch>(0x2100000000);

    sc.insert(s);

    EXPECT_NE(sc.getSwitch(0x2100000000), nullptr);

    EXPECT_EQ(sc.getSwitch(1), nullptr);
}
