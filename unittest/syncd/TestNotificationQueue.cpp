#include "NotificationQueue.h"

#include "sairediscommon.h"

#include <gtest/gtest.h>

using namespace syncd;

static std::string fdbData =
"[{\"fdb_entry\":\"{\\\"bvid\\\":\\\"oid:0x260000000005be\\\",\\\"mac\\\":\\\"52:54:00:86:DD:7A\\\",\\\"switch_id\\\":\\\"oid:0x21000000000000\\\"}\","
"\"fdb_event\":\"SAI_FDB_EVENT_LEARNED\","
"\"list\":[{\"id\":\"SAI_FDB_ENTRY_ATTR_TYPE\",\"value\":\"SAI_FDB_ENTRY_TYPE_DYNAMIC\"},{\"id\":\"SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID\",\"value\":\"oid:0x3a000000000660\"}]}]";

static std::string sscData = "{\"status\":\"SAI_SWITCH_OPER_STATUS_UP\",\"switch_id\":\"oid:0x2100000000\"}";

TEST(NotificationQueue, EnqueueLimitTest)
{
    bool status;
    int i;
    std::vector<swss::FieldValueTuple> fdbEntry;
    std::vector<swss::FieldValueTuple> sscEntry;

    // Set up a queue with limit at 5 and threshold at 3 where after this is reached event starts dropping
    syncd::NotificationQueue testQ(5, 3);

    // Try queue up 5 fake FDB event and expect them to be added successfully
    swss::KeyOpFieldsValuesTuple fdbItem(SAI_SWITCH_NOTIFICATION_NAME_FDB_EVENT, fdbData, fdbEntry);
    for (i = 0; i < 5; ++i)
    {
        status = testQ.enqueue(fdbItem);
        EXPECT_EQ(status, true);
    }

    // On the 6th fake FDB event expect it to be dropped right away
    status = testQ.enqueue(fdbItem);
    EXPECT_EQ(status, false);

    // Add 2 switch state change events expect both are accepted as consecutive limit not yet reached
    swss::KeyOpFieldsValuesTuple sscItem(SAI_SWITCH_NOTIFICATION_NAME_SWITCH_STATE_CHANGE, sscData, sscEntry);
    for (i = 0; i < 2; ++i)
    {
        status = testQ.enqueue(sscItem);
        EXPECT_EQ(status, true);
    }

    // On the 3rd consecutive switch state change event expect it to be dropped
    status = testQ.enqueue(sscItem);
    EXPECT_EQ(status, false);

    // Add a fake FDB event to cause the consecutive event signature to change while this FDB event is dropped
    status = testQ.enqueue(fdbItem);
    EXPECT_EQ(status, false);

    // Add 2 switch state change events expect both are accepted as consecutive limit not yet reached
    for (i = 0; i < 2; ++i)
    {
        status = testQ.enqueue(sscItem);
        EXPECT_EQ(status, true);
    }

    // Add 2 switch state change events expect both are dropped as consecutive limit has already reached
    for (i = 0; i < 2; ++i)
    {
        status = testQ.enqueue(sscItem);
        EXPECT_EQ(status, false);
    }
}

