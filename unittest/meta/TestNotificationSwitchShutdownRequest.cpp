#include "NotificationSwitchShutdownRequest.h"
#include "Meta.h"
#include "MetaTestSaiInterface.h"

#include "sairediscommon.h"
#include "sai_serialize.h"

#include <gtest/gtest.h>

#include <memory>

using namespace sairedis;
using namespace saimeta;

static std::string s = "{\"switch_id\":\"oid:0x2100000000\"}";
static std::string null = "{\"switch_id\":\"oid:0x0\"}";

TEST(NotificationSwitchShutdownRequest, ctr)
{
    NotificationSwitchShutdownRequest n(s);
}

TEST(NotificationSwitchShutdownRequest, getSwitchId)
{
    NotificationSwitchShutdownRequest n(s);

    EXPECT_EQ(n.getSwitchId(), 0x2100000000);

    NotificationSwitchShutdownRequest n2(null);

    EXPECT_EQ(n2.getSwitchId(), 0);
}

TEST(NotificationSwitchShutdownRequest, getAnyObjectId)
{
    NotificationSwitchShutdownRequest n(s);

    EXPECT_EQ(n.getAnyObjectId(), 0x2100000000);

    NotificationSwitchShutdownRequest n2(null);

    EXPECT_EQ(n2.getAnyObjectId(), 0);
}

TEST(NotificationSwitchShutdownRequest, processMetadata)
{
    NotificationSwitchShutdownRequest n(s);

    auto sai = std::make_shared<MetaTestSaiInterface>();
    auto meta = std::make_shared<Meta>(sai);

    n.processMetadata(meta);
}

static void on_switch_shutdown_request_notification(
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();
}

TEST(NotificationSwitchShutdownRequest, executeCallback)
{
    NotificationSwitchShutdownRequest n(s);

    sai_switch_notifications_t ntfs;

    ntfs.on_switch_shutdown_request = &on_switch_shutdown_request_notification;

    n.executeCallback(ntfs);
}
