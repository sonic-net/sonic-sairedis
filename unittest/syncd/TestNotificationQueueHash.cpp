#include "NotificationQueueHash.h"

#include <gtest/gtest.h>

#include <memory>

using namespace syncd;

TEST(NotificationQueueHash, enqueue)
{
    NotificationQueueHash hash;

    swss::KeyOpFieldsValuesTuple kco;

    EXPECT_EQ(hash.enqueue("foo", kco), true);
    EXPECT_EQ(hash.enqueue("foo", kco), true);

    for (int i = 0; i < 2000; i++)
    {
        EXPECT_EQ(hash.enqueue("foo", kco), true);
    }
}

TEST(NotificationQueueHash, tryDequeue)
{
    NotificationQueueHash hash;

    swss::KeyOpFieldsValuesTuple kco;

    EXPECT_EQ(hash.tryDequeue(kco), false);

    EXPECT_EQ(hash.enqueue("foo", kco), true);

    EXPECT_EQ(hash.tryDequeue(kco), true);
}
