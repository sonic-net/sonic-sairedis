#include "SwitchConfigContainer.h"

#include <gtest/gtest.h>

#include <memory>

using namespace sairedis;

TEST(SwitchConfigContainer, insert)
{
    auto sc = std::make_shared<SwitchConfig>(0, "");
    auto sc1 = std::make_shared<SwitchConfig>(1, "");

    auto scc = std::make_shared<SwitchConfigContainer>();

    scc->insert(sc);

    EXPECT_THROW(scc->insert(sc), std::runtime_error);
    EXPECT_THROW(scc->insert(sc1), std::runtime_error);
}
    
TEST(SwitchConfigContainer, getConfig)
{
    auto sc0 = std::make_shared<SwitchConfig>(0, "0");
    auto sc1 = std::make_shared<SwitchConfig>(1, "1");

    auto scc = std::make_shared<SwitchConfigContainer>();

    scc->insert(sc0);
    scc->insert(sc1);

    EXPECT_EQ(scc->getConfig(7), nullptr);

    EXPECT_NE(scc->getConfig(1), nullptr);
    EXPECT_NE(scc->getConfig(0), nullptr);
}
