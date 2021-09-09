#include "Switch.h"

#include <gtest/gtest.h>

#include <memory>

using namespace sairedis;

TEST(Switch, ctr)
{
    EXPECT_THROW(std::make_shared<Switch>(SAI_NULL_OBJECT_ID), std::runtime_error);
}
