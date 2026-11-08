#include "PortRelatedSet.h"

#include <gtest/gtest.h>

#include <memory>

using namespace saimeta;

TEST(PortRelatedSet, inc)
{
    PortRelatedSet set;

    set.insert(0, 0);

    EXPECT_EQ(set.getAllPorts().size(), 0);

    EXPECT_THROW(set.insert(0, 1), std::runtime_error);
}
