#include "EventQueue.h"

#include <linux/if.h>

#include <gtest/gtest.h>

using namespace saivs;

TEST(EventQueue, ctr)
{
    EXPECT_THROW(std::make_shared<EventQueue>(nullptr), std::runtime_error);

    auto s = std::make_shared<Signal>();

    EventQueue eq(s);
}

TEST(EventQueue, size)
{
    auto s = std::make_shared<Signal>();

    EventQueue eq(s);

    EXPECT_EQ(eq.size(), 0);
}
