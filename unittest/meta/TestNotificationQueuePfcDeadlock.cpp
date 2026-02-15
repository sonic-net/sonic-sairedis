#include "NotificationQueuePfcDeadlock.h"
#include "Meta.h"
#include "MetaTestSaiInterface.h"

#include "sairediscommon.h"
#include "sai_serialize.h"

#include <gtest/gtest.h>

#include <memory>

using namespace sairedis;
using namespace saimeta;

static std::string s = "[{\"event\":\"SAI_QUEUE_PFC_DEADLOCK_EVENT_TYPE_DETECTED\",\"queue_id\":\"oid:0x1500000000020a\"}]";
static std::string null = "[{\"event\":\"SAI_QUEUE_PFC_DEADLOCK_EVENT_TYPE_DETECTED\",\"queue_id\":\"oid:0x0\"}]";
static std::string fullnull = "[]";

TEST(NotificationQueuePfcDeadlock, ctr)
{
    NotificationQueuePfcDeadlock n(s);
}

TEST(NotificationQueuePfcDeadlock, getSwitchId)
{
    NotificationQueuePfcDeadlock n(s);

    EXPECT_EQ(n.getSwitchId(), 0);

    NotificationQueuePfcDeadlock n2(null);

    EXPECT_EQ(n2.getSwitchId(), 0);
}

TEST(NotificationQueuePfcDeadlock, getAnyObjectId)
{
    NotificationQueuePfcDeadlock n(s);

    EXPECT_EQ(n.getAnyObjectId(), 0x1500000000020a);

    NotificationQueuePfcDeadlock n2(null);

    EXPECT_EQ(n2.getAnyObjectId(), 0);

    NotificationQueuePfcDeadlock n3(fullnull);

    EXPECT_EQ(n3.getSwitchId(), 0);

    EXPECT_EQ(n3.getAnyObjectId(), 0);
}

TEST(NotificationQueuePfcDeadlock, processMetadata)
{
    NotificationQueuePfcDeadlock n(s);

    auto sai = std::make_shared<MetaTestSaiInterface>();
    auto meta = std::make_shared<Meta>(sai);

    n.processMetadata(meta);
}

static void on_queue_pfc_deadlock_notification(
        _In_ uint32_t count,
        _In_ const sai_queue_deadlock_notification_data_t *data)
{
    SWSS_LOG_ENTER();
}

TEST(NotificationQueuePfcDeadlock, executeCallback)
{
    NotificationQueuePfcDeadlock n(s);

    sai_switch_notifications_t ntfs;

    ntfs.on_queue_pfc_deadlock = &on_queue_pfc_deadlock_notification;

    n.executeCallback(ntfs);
}

