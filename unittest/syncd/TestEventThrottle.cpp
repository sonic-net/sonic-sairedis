#include "EventThrottle.h"

#include <gtest/gtest.h>

#include <memory>

using namespace syncd;

TEST(EventThrottle, getTime)
{
    EXPECT_NE(EventThrottle::getTime(), 0);
}

TEST(EventThrottle, shouldEnqueue)
{
    EventThrottle et("test");

    for (int i = 0; i < 10; i++)
    {
        EXPECT_TRUE(et.shouldEnqueue());
    }

    for (int i = 0; i < 100; i++)
    {
        EXPECT_FALSE(et.shouldEnqueue());
    }

    sleep(1);

    EXPECT_TRUE(et.shouldEnqueue());
}
