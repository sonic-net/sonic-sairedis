#include "NotificationFdbEvent.h"
#include "Meta.h"
#include "MetaTestSaiInterface.h"

#include "sairediscommon.h"
#include "sai_serialize.h"

#include <gtest/gtest.h>

#include <memory>

using namespace sairedis;
using namespace saimeta;

static std::string s = 
"[{\"fdb_entry\":\"{\\\"bvid\\\":\\\"oid:0x260000000005be\\\",\\\"mac\\\":\\\"52:54:00:86:DD:7A\\\",\\\"switch_id\\\":\\\"oid:0x21000000000000\\\"}\","
"\"fdb_event\":\"SAI_FDB_EVENT_LEARNED\","
"\"list\":[{\"id\":\"SAI_FDB_ENTRY_ATTR_TYPE\",\"value\":\"SAI_FDB_ENTRY_TYPE_DYNAMIC\"},{\"id\":\"SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID\",\"value\":\"oid:0x3a000000000660\"}]}]";

static std::string null = 
"[{\"fdb_entry\":\"{\\\"bvid\\\":\\\"oid:0x260000000005be\\\",\\\"mac\\\":\\\"52:54:00:86:DD:7A\\\",\\\"switch_id\\\":\\\"oid:0x0\\\"}\","
"\"fdb_event\":\"SAI_FDB_EVENT_LEARNED\","
"\"list\":[{\"id\":\"SAI_FDB_ENTRY_ATTR_TYPE\",\"value\":\"SAI_FDB_ENTRY_TYPE_DYNAMIC\"},{\"id\":\"SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID\",\"value\":\"oid:0x3a000000000660\"}]}]";

static std::string fullnull = "[]";

TEST(NotificationFdbEvent, ctr)
{
    NotificationFdbEvent n(s);
}

TEST(NotificationFdbEvent, getSwitchId)
{
    NotificationFdbEvent n(s);

    EXPECT_EQ(n.getSwitchId(), 0x21000000000000);

    NotificationFdbEvent n2(null);

    EXPECT_EQ(n2.getSwitchId(), 0);
}

TEST(NotificationFdbEvent, getAnyObjectId)
{
    NotificationFdbEvent n(s);

    EXPECT_EQ(n.getAnyObjectId(), 0x21000000000000);

    NotificationFdbEvent n2(null);

    EXPECT_EQ(n2.getAnyObjectId(), 0x260000000005be);

    NotificationFdbEvent n3(fullnull);

    EXPECT_EQ(n3.getSwitchId(), 0x0);

    EXPECT_EQ(n3.getAnyObjectId(), 0x0);
}

TEST(NotificationFdbEvent, processMetadata)
{
    NotificationFdbEvent n(s);

    auto sai = std::make_shared<MetaTestSaiInterface>();
    auto meta = std::make_shared<Meta>(sai);

    n.processMetadata(meta);
}
