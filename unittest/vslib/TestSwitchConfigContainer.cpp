#include "SwitchConfigContainer.h"

#include <gtest/gtest.h>

#include <memory>

using namespace saivs;

TEST(SwitchConfigContainer, insert)
{
    SwitchConfigContainer scc;

    EXPECT_THROW(scc.insert(nullptr), std::runtime_error);

    auto sc0 = std::make_shared<SwitchConfig>(0,"foo");
    auto sc1 = std::make_shared<SwitchConfig>(0,"bar");
    auto sc2 = std::make_shared<SwitchConfig>(1,"bar");

    scc.insert(sc1);

    EXPECT_THROW(scc.insert(sc0), std::runtime_error);

    EXPECT_THROW(scc.insert(sc2), std::runtime_error);
}

TEST(SwitchConfigContainer, getConfig)
{
    SwitchConfigContainer scc;

    auto sc0 = std::make_shared<SwitchConfig>(0,"foo");
    auto sc1 = std::make_shared<SwitchConfig>(1,"bar");

    scc.insert(sc0);
    scc.insert(sc1);

    EXPECT_EQ(scc.getConfig(2), nullptr);
    EXPECT_NE(scc.getConfig(0), nullptr);
    EXPECT_NE(scc.getConfig(1), nullptr);
}
