#include "Switch.h"

#include <gtest/gtest.h>

using namespace sairedis;

TEST(Switch, ctr)
{
    EXPECT_THROW(new Switch(SAI_NULL_OBJECT_ID), std::runtime_error);
}
