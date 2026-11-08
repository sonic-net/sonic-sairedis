#include "MetaTestSaiInterface.h"
#include "NotificationHaScopeEvent.h"
#include "Meta.h"

#include "sairediscommon.h"
#include "sai_serialize.h"

#include <gtest/gtest.h>

using namespace sairedis;
using namespace saimeta;

static std::string s = "[{\"event_type\": \"SAI_HA_SCOPE_EVENT_STATE_CHANGED\",\"ha_scope_id\":\"oid:0x123456789abcdef\",\"ha_role\":\"SAI_DASH_HA_ROLE_ACTIVE\",\"flow_version\":\"1\",\"ha_state\":\"SAI_DASH_HA_STATE_ACTIVE\"}]";
static std::string null = "[{\"event_type\": \"SAI_HA_SCOPE_EVENT_STATE_CHANGED\",\"ha_scope_id\":\"oid:0x0\",\"ha_role\":\"SAI_DASH_HA_ROLE_ACTIVE\",\"flow_version\":\"1\",\"ha_state\":\"SAI_DASH_HA_STATE_ACTIVE\"}]";
static std::string fullnull = "[]";


TEST(NotificationHaEvent, ha_scope)
{
    NotificationHaScopeEvent n(s);
}

TEST(NotificationHaEvent, getSwitchId)
{
    NotificationHaScopeEvent n(s);

    EXPECT_EQ(n.getSwitchId(), SAI_NULL_OBJECT_ID);

    NotificationHaScopeEvent n2(null);

    EXPECT_EQ(n2.getSwitchId(), SAI_NULL_OBJECT_ID);

    NotificationHaScopeEvent n3(fullnull);

    EXPECT_EQ(n3.getSwitchId(), SAI_NULL_OBJECT_ID);
}

TEST(NotificationHaEvent, getAnyObjectId)
{
    NotificationHaScopeEvent n(s);

    EXPECT_EQ(n.getAnyObjectId(), 0x123456789abcdef);

    NotificationHaScopeEvent n2(null);

    EXPECT_EQ(n2.getAnyObjectId(), SAI_NULL_OBJECT_ID);

    NotificationHaScopeEvent n3(fullnull);

    EXPECT_EQ(n3.getAnyObjectId(), SAI_NULL_OBJECT_ID);
}

TEST(NotificationHaEvent, processMetadata)
{
    NotificationHaScopeEvent n(s);

    auto sai = std::make_shared<MetaTestSaiInterface>();
    auto meta = std::make_shared<Meta>(sai);

    n.processMetadata(meta);
}

static void on_ha_scope_event(
        _In_ uint32_t count,
        _In_ const sai_ha_scope_event_data_t* data)
{
    SWSS_LOG_ENTER();
}

TEST(NotificationHaEvent, executeCallback)
{
    NotificationHaScopeEvent n(s);

    sai_switch_notifications_t switchNotifications;
    switchNotifications.on_ha_scope_event = &on_ha_scope_event;

    n.executeCallback(switchNotifications);
}
