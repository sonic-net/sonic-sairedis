#include "EventPayloadPlatformEvent.h"

#include <gtest/gtest.h>

using namespace saivs;

TEST(EventPayloadPlatformEvent, ctr)
{
    EventPayloadPlatformEvent ep(0);
}

TEST(EventPayloadPlatformEvent, getSwitchId)
{
    EventPayloadPlatformEvent ep(0x21000000000000);

    EXPECT_EQ(ep.getSwitchId(), 0x21000000000000);
}
