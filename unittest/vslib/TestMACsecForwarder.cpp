#include "MACsecForwarder.h"

#include <gtest/gtest.h>

#include <memory>

using namespace saivs;

TEST(MACsecForwarder, ctr)
{
    EXPECT_THROW(std::make_shared<MACsecForwarder>("foo", nullptr), std::runtime_error);

    auto s = std::make_shared<Signal>();

    auto q = std::make_shared<EventQueue>(s);
}
