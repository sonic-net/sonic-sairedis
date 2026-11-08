#include "SwitchContainer.h"

#include <gtest/gtest.h>

#include <memory>

using namespace sairedis;

TEST(SwitchContainer, insert)
{
    auto s = std::make_shared<Switch>(1);

    auto sc = std::make_shared<SwitchContainer>();

    sc->insert(s);

    EXPECT_THROW(sc->insert(s), std::runtime_error);

    auto s2 = std::make_shared<Switch>(2);

    EXPECT_THROW(sc->insert(s2), std::runtime_error);
}

TEST(SwitchContainer, removeSwitch)
{
    auto s = std::make_shared<Switch>(1);

    auto sc = std::make_shared<SwitchContainer>();

    sc->insert(s);

    EXPECT_THROW(sc->removeSwitch(2), std::runtime_error);

    sc->removeSwitch(1);
}

TEST(SwitchContainer, removeSwitch_shared)
{
    auto s = std::make_shared<Switch>(1);

    auto sc = std::make_shared<SwitchContainer>();

    sc->insert(s);

    auto s2 = std::make_shared<Switch>(2);

    EXPECT_THROW(sc->removeSwitch(s2), std::runtime_error);

    sc->removeSwitch(s);
}

TEST(SwitchContainer, getSwitch)
{
    auto s = std::make_shared<Switch>(1);

    auto sc = std::make_shared<SwitchContainer>();

    sc->insert(s);

    EXPECT_NE(sc->getSwitch(1), nullptr);

    EXPECT_EQ(sc->getSwitch(2), nullptr);
}

TEST(SwitchContainer, clear)
{
    auto s = std::make_shared<Switch>(1);

    auto sc = std::make_shared<SwitchContainer>();

    sc->insert(s);

    EXPECT_NE(sc->getSwitch(1), nullptr);

    sc->clear();

    EXPECT_EQ(sc->getSwitch(1), nullptr);
}

TEST(SwitchContainer, contains)
{
    auto s = std::make_shared<Switch>(1);

    auto sc = std::make_shared<SwitchContainer>();

    sc->insert(s);

    EXPECT_TRUE(sc->contains(1));

    EXPECT_FALSE(sc->contains(2));
}

TEST(SwitchContainer, getSwitchByHardwareInfo)
{
    auto s = std::make_shared<Switch>(1);

    auto sc = std::make_shared<SwitchContainer>();

    sc->insert(s);

    EXPECT_EQ(sc->getSwitchByHardwareInfo("foo"), nullptr);

    EXPECT_NE(sc->getSwitchByHardwareInfo(""), nullptr);
}

