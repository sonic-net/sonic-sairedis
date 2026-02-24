#include "NotificationFlowBulkGetSessionEvent.h"
#include "Meta.h"
#include "MetaTestSaiInterface.h"

#include "sairediscommon.h"
#include "sai_serialize.h"

#include <gtest/gtest.h>

#include <memory>

using namespace sairedis;
using namespace saimeta;

static std::string s = "{\"bulk_session_id\":\"oid:0x123456789abcdef\",\"data\":[{\"event_type\":\"SAI_FLOW_BULK_GET_SESSION_EVENT_FINISHED\"}]}";
static std::string null = "{\"bulk_session_id\":\"oid:0x0\",\"data\":[{\"event_type\":\"SAI_FLOW_BULK_GET_SESSION_EVENT_FINISHED\"}]}";
static std::string emptydata = "{\"bulk_session_id\":\"oid:0x123456789abcdef\",\"data\":[]}";

TEST(NotificationFlowBulkGetSessionEvent, ctr)
{
    NotificationFlowBulkGetSessionEvent n(s);
}

TEST(NotificationFlowBulkGetSessionEvent, getSwitchId)
{
    NotificationFlowBulkGetSessionEvent n(s);

    EXPECT_EQ(n.getSwitchId(), SAI_NULL_OBJECT_ID);

    NotificationFlowBulkGetSessionEvent n2(null);

    EXPECT_EQ(n2.getSwitchId(), SAI_NULL_OBJECT_ID);

    NotificationFlowBulkGetSessionEvent n3(emptydata);

    EXPECT_EQ(n3.getSwitchId(), SAI_NULL_OBJECT_ID);
}

TEST(NotificationFlowBulkGetSessionEvent, getAnyObjectId)
{
    NotificationFlowBulkGetSessionEvent n(s);

    EXPECT_EQ(n.getAnyObjectId(), 0x123456789abcdef);

    NotificationFlowBulkGetSessionEvent n2(null);

    EXPECT_EQ(n2.getAnyObjectId(), SAI_NULL_OBJECT_ID);

    NotificationFlowBulkGetSessionEvent n3(emptydata);

    EXPECT_EQ(n3.getAnyObjectId(), 0x123456789abcdef);
}

TEST(NotificationFlowBulkGetSessionEvent, processMetadata)
{
    NotificationFlowBulkGetSessionEvent n(s);

    auto sai = std::make_shared<MetaTestSaiInterface>();
    auto meta = std::make_shared<Meta>(sai);

    n.processMetadata(meta);
}

static void on_flow_bulk_get_session_event(
        _In_ sai_object_id_t flow_bulk_session_id,
        _In_ uint32_t count,
        _In_ const sai_flow_bulk_get_session_event_data_t* data)
{
    SWSS_LOG_ENTER();
}

TEST(NotificationFlowBulkGetSessionEvent, executeCallback)
{
    NotificationFlowBulkGetSessionEvent n(s);

    sai_switch_notifications_t switchNotifications;
    switchNotifications.on_flow_bulk_get_session_event = &on_flow_bulk_get_session_event;

    n.executeCallback(switchNotifications);
}


