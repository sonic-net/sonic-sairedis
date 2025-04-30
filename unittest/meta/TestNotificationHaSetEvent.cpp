#include "MetaTestSaiInterface.h"
#include "NotificationHaSetEvent.h"
#include "Meta.h"

#include "sairediscommon.h"
#include "sai_serialize.h"

#include <gtest/gtest.h>

using namespace sairedis;
using namespace saimeta;

static std::string s = "[{\"event_type\": \"SAI_HA_SET_EVENT_DP_CHANNEL_UP\",\"ha_set_id\":\"oid:0x123456789abcdef\"}]";
static std::string null = "[{\"event_type\": \"SAI_HA_SET_EVENT_DP_CHANNEL_UP\",\"ha_set_id\":\"oid:0x0\"}]";
static std::string fullnull = "[]";

TEST(NotificationHaSetEvent, ha_set)
{
    NotificationHaSetEvent n(s);
}

TEST(NotificationHaSetEvent, getSwitchId)
{
    NotificationHaSetEvent n(s);

    EXPECT_EQ(n.getSwitchId(), SAI_NULL_OBJECT_ID);

    NotificationHaSetEvent n2(null);

    EXPECT_EQ(n2.getSwitchId(), SAI_NULL_OBJECT_ID);

    NotificationHaSetEvent n3(fullnull);

    EXPECT_EQ(n3.getSwitchId(), SAI_NULL_OBJECT_ID);
}

TEST(NotificationHaSetEvent, getAnyObjectId)
{
    NotificationHaSetEvent n(s);

    EXPECT_EQ(n.getAnyObjectId(), 0x123456789abcdef);

    NotificationHaSetEvent n2(null);

    EXPECT_EQ(n2.getAnyObjectId(), SAI_NULL_OBJECT_ID);

    NotificationHaSetEvent n3(fullnull);

    EXPECT_EQ(n3.getAnyObjectId(), SAI_NULL_OBJECT_ID);
}

TEST(NotificationHaSetEvent, processMetadata)
{
    NotificationHaSetEvent n(s);

    auto sai = std::make_shared<MetaTestSaiInterface>();
    auto meta = std::make_shared<Meta>(sai);

    n.processMetadata(meta);
}

static void on_ha_set_event(
        _In_ uint32_t count,
        _In_ const sai_ha_set_event_data_t* data)
{
    SWSS_LOG_ENTER();
}

TEST(NotificationHaSetEvent, executeCallback)
{
    NotificationHaSetEvent n(s);

    sai_switch_notifications_t switchNotifications;

    switchNotifications.on_ha_set_event = &on_ha_set_event;

    n.executeCallback(switchNotifications);
}
