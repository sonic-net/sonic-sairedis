#include "ContextConfigContainer.h"

#include <gtest/gtest.h>

#include <memory>

using namespace saivs;

TEST(ContextConfigContainer, insert)
{
    auto cc = std::make_shared<ContextConfig>(1, "foo", "ASIC_DB");

    ContextConfigContainer ccc;

    ccc.insert(cc);

    EXPECT_EQ(ccc.get(2), nullptr);
    EXPECT_NE(ccc.get(1), nullptr);
}

TEST(ContextConfigContainer, loadFromFile)
{
    // on non existing file, default should be loaded

    auto ccc = ContextConfigContainer::loadFromFile("foo");

    EXPECT_NE(ccc, nullptr);
    EXPECT_NE(ccc->get(0), nullptr);
    EXPECT_EQ(ccc->get(0)->m_guid, 0);
    EXPECT_EQ(ccc->get(0)->m_name, "VirtualSwitch");

    ccc = ContextConfigContainer::loadFromFile("files/context_config.bad.json");

    EXPECT_NE(ccc, nullptr);
    EXPECT_NE(ccc->get(0), nullptr);
    EXPECT_EQ(ccc->get(0)->m_guid, 0);
    EXPECT_EQ(ccc->get(0)->m_name, "VirtualSwitch");

    ccc = ContextConfigContainer::loadFromFile("files/context_config.good.json");

    EXPECT_NE(ccc, nullptr);
    EXPECT_NE(ccc->get(0), nullptr);
    EXPECT_EQ(ccc->get(0)->m_guid, 0);
    EXPECT_EQ(ccc->get(0)->m_name, "syncd");

}
