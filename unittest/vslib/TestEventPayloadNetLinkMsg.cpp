#include "EventPayloadNetLinkMsg.h"

#include <linux/if.h>

#include <gtest/gtest.h>

using namespace saivs;

TEST(EventPayloadNetLinkMsg, ctr)
{
    EventPayloadNetLinkMsg ep(0, 1, 2, IFF_UP, "foo");
}

TEST(EventPayloadNetLinkMsg, getSwitchId)
{
    EventPayloadNetLinkMsg ep(0, 1, 2, IFF_UP, "foo");

    EXPECT_EQ(ep.getSwitchId(), 0);
}

TEST(EventPayloadNetLinkMsg, getNlmsgType)
{
    EventPayloadNetLinkMsg ep(0, 1, 2, IFF_UP, "foo");

    EXPECT_EQ(ep.getNlmsgType(), 1);
}

TEST(EventPayloadNetLinkMsg, getIfIndex)
{
    EventPayloadNetLinkMsg ep(0, 1, 2, IFF_UP, "foo");

    EXPECT_EQ(ep.getIfIndex(), 2);
}

TEST(EventPayloadNetLinkMsg, getIfFlags)
{
    EventPayloadNetLinkMsg ep(0, 1, 2, IFF_UP, "foo");

    EXPECT_EQ(ep.getIfFlags(), IFF_UP);
}

TEST(EventPayloadNetLinkMsg, getIfName)
{
    EventPayloadNetLinkMsg ep(0, 1, 2, IFF_UP, "foo");

    EXPECT_EQ(ep. getIfName(), "foo");
}
