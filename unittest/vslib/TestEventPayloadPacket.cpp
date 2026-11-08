#include "EventPayloadPacket.h"

#include <linux/if.h>

#include <gtest/gtest.h>

using namespace saivs;

TEST(EventPayloadPacket, ctr)
{
    uint8_t data[2] = { 1, 2 };

    Buffer b(data, 2);

    EventPayloadPacket ep(0, 1, "foo", b);
}

TEST(EventPayloadPacket, getPort)
{
    uint8_t data[2] = { 1, 2 };
    Buffer b(data, 2);
    EventPayloadPacket ep(0, 1, "foo", b);

    EXPECT_EQ(ep.getPort(), 0);
}

TEST(EventPayloadPacket, getIfIndex)
{
    uint8_t data[2] = { 1, 2 };
    Buffer b(data, 2);
    EventPayloadPacket ep(0, 1, "foo", b);

    EXPECT_EQ(ep.getIfIndex(), 1);
}

TEST(EventPayloadPacket, getIfName)
{
    uint8_t data[2] = { 1, 2 };
    Buffer b(data, 2);
    EventPayloadPacket ep(0, 1, "foo", b);

    EXPECT_EQ(ep.getIfName(), "foo");
}

TEST(EventPayloadPacket, getBuffer)
{
    uint8_t data[2] = { 1, 2 };
    Buffer b(data, 2);
    EventPayloadPacket ep(0, 1, "foo", b);

    auto&bb = ep.getBuffer();

    EXPECT_EQ(bb.getData()[0], 1);
    EXPECT_EQ(bb.getData()[1], 2);
}
