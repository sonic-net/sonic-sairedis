#include "ContextConfigContainer.h"

#include "swss/logger.h"

#include <gtest/gtest.h>

using namespace sairedis;

TEST(ContextConfigContainer, get)
{
    ContextConfigContainer ccc;

    EXPECT_EQ(ccc.get(0), nullptr);
}

TEST(ContextConfigContainer, loadFromFile)
{
    ContextConfigContainer ccc;

    EXPECT_NE(ccc.loadFromFile("files/ccc_bad.txt"), nullptr);
}

TEST(ContextConfigContainer, insert)
{
    ContextConfigContainer ccc;

    auto cc = std::make_shared<ContextConfig>(0, "syncd", "ASIC_DB", "COUNTERS_DB","FLEX_DB", "STATE_DB");

    ccc.insert(cc);

    EXPECT_THROW(ccc.insert(cc), std::runtime_error);
}
