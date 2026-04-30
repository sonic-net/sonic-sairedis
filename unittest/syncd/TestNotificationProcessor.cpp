#include "VirtualOidTranslator.h"
#include "RedisClient.h"
#include "RedisNotificationProducer.h"
#include "NotificationProcessor.h"
#include "lib/RedisVidIndexGenerator.h"
#include "lib/sairediscommon.h"
#include "vslib/Sai.h"

#include "swss/table.h"
#include "swss/logger.h"

#include <gtest/gtest.h>

using namespace syncd;

static std::string natData =
"[{\"nat_entry\":\"{\\\"nat_data\\\":{\\\"key\\\":{\\\"dst_ip\\\":\\\"10.10.10.10\\\",\\\"l4_dst_port\\\":\\\"20006\\\",\\\"l4_src_port\\\":\\\"0\\\",\\\"proto\\\":\\\"6\\\",\\\"src_ip\\\":\\\"0.0.0.0\\\"},\\\"mask\\\":{\\\"dst_ip\\\":\\\"255.255.255.255\\\",\\\"l4_dst_port\\\":\\\"65535\\\",\\\"l4_src_port\\\":\\\"0\\\",\\\"proto\\\":\\\"255\\\",\\\"src_ip\\\":\\\"0.0.0.0\\\"}},\\\"nat_type\\\":\\\"SAI_NAT_TYPE_DESTINATION_NAT\\\",\\\"switch_id\\\":\\\"oid:0x21000000000000\\\",\\\"vr\\\":\\\"oid:0x3000000000048\\\"}\",\"nat_event\":\"SAI_NAT_EVENT_AGED\"}]";

static std::string icmp_echo_session_ntf_str = "[{\"icmp_echo_session_id\":\"oid:0x100000000003a\",\"session_state\":\"SAI_ICMP_ECHO_SESSION_STATE_DOWN\"}]";

TEST(NotificationProcessor, NotificationProcessorTest)
{
    auto sai = std::make_shared<saivs::Sai>();
    auto dbAsic = std::make_shared<swss::DBConnector>("ASIC_DB", 0);
    auto client = std::make_shared<RedisClient>(dbAsic);
    auto producer = std::make_shared<syncd::RedisNotificationProducer>("ASIC_DB");

    auto notificationProcessor = std::make_shared<NotificationProcessor>(producer, client,
                                                             [](const swss::KeyOpFieldsValuesTuple&){});
    EXPECT_NE(notificationProcessor, nullptr);

    auto switchConfigContainer = std::make_shared<sairedis::SwitchConfigContainer>();
    auto redisVidIndexGenerator = std::make_shared<sairedis::RedisVidIndexGenerator>(dbAsic, REDIS_KEY_VIDCOUNTER);
    EXPECT_NE(redisVidIndexGenerator, nullptr);

    auto virtualObjectIdManager = std::make_shared<sairedis::VirtualObjectIdManager>(0, switchConfigContainer, redisVidIndexGenerator);
    EXPECT_NE(virtualObjectIdManager, nullptr);

    auto translator = std::make_shared<VirtualOidTranslator>(client,
                                                             virtualObjectIdManager,
                                                             sai);
    EXPECT_NE(translator, nullptr);
    notificationProcessor->m_translator = translator;

    // Check NAT notification without RIDs
    std::vector<swss::FieldValueTuple> natEntry;
    swss::KeyOpFieldsValuesTuple natFV(SAI_SWITCH_NOTIFICATION_NAME_NAT_EVENT, natData, natEntry);
    notificationProcessor->syncProcessNotification(natFV);

    // Check NAT notification with RIDs present
    translator->insertRidAndVid(0x21000000000000,0x210000000000);
    translator->insertRidAndVid(0x3000000000048,0x30000000048);

    notificationProcessor->syncProcessNotification(natFV);

    translator->eraseRidAndVid(0x21000000000000,0x210000000000);
    translator->eraseRidAndVid(0x3000000000048,0x30000000048);

    // Test FDB MOVE event
    std::string key = "ASIC_STATE:SAI_OBJECT_TYPE_FDB_ENTRY:{\"bvid\":\"oid:0x26000000000001\",\"mac\":\"00:00:00:00:00:01\",\"switch_id\":\"oid:0x210000000000\"}";
    dbAsic->hset(key, "SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID", "oid:0x3a000000000a98");
    dbAsic->hset(key, "SAI_FDB_ENTRY_ATTR_TYPE", "SAI_FDB_ENTRY_TYPE_STATIC");
    dbAsic->hset(key, "SAI_FDB_ENTRY_ATTR_ENDPOINT_IP", "10.0.0.1");

    translator->insertRidAndVid(0x21000000000000,0x210000000000);
    translator->insertRidAndVid(0x1003a0000004a,0x3a000000000a99);
    translator->insertRidAndVid(0x2600000001,0x26000000000001);

    static std::string fdb_data = "[{\"fdb_entry\":\"{\\\"bvid\\\":\\\"oid:0x2600000001\\\",\\\"mac\\\":\\\"00:00:00:00:00:01\\\",\\\"switch_id\\\":\\\"oid:0x21000000000000\\\"}\",\"fdb_event\":\"SAI_FDB_EVENT_MOVE\",\"list\":[{\"id\":\"SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID\",\"value\":\"oid:0x1003a0000004a\"}]}]";
    std::vector<swss::FieldValueTuple> fdb_entry;
    swss::KeyOpFieldsValuesTuple item(SAI_SWITCH_NOTIFICATION_NAME_FDB_EVENT, fdb_data, fdb_entry);

    notificationProcessor->syncProcessNotification(item);
    translator->eraseRidAndVid(0x21000000000000,0x210000000000);
    translator->eraseRidAndVid(0x1003a0000004a,0x3a000000000a99);
    translator->eraseRidAndVid(0x2600000001,0x26000000000001);
    auto bridgeport = dbAsic->hget(key, "SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID");
    auto ip = dbAsic->hget(key, "SAI_FDB_ENTRY_ATTR_ENDPOINT_IP");
    EXPECT_NE(bridgeport, nullptr);
    EXPECT_EQ(*bridgeport, "oid:0x3a000000000a99");
    EXPECT_EQ(ip, nullptr);

    //Test ICMP_ECHO_SESSION_STATE_CHANGE Notification
    translator->insertRidAndVid(0x21000000000000,0x210000000000);
    translator->insertRidAndVid(0x100000000003a,0x100000000003a);
    std::vector<swss::FieldValueTuple> icmp_echo_session_ntf_entry;
    swss::KeyOpFieldsValuesTuple icmp_obj(SAI_SWITCH_NOTIFICATION_NAME_ICMP_ECHO_SESSION_STATE_CHANGE, icmp_echo_session_ntf_str, icmp_echo_session_ntf_entry);
    notificationProcessor->syncProcessNotification(icmp_obj);
    translator->eraseRidAndVid(0x21000000000000,0x210000000000);
    translator->insertRidAndVid(0x100000000003a,0x100000000003a);

    // Test ASIC/SDK health event
    std::string asheString = "{"
        "\"category\":\"SAI_SWITCH_ASIC_SDK_HEALTH_CATEGORY_FW\","
        "\"data.data_type\":\"SAI_HEALTH_DATA_TYPE_GENERAL\","
        "\"description\":\"2:30,30\","
        "\"severity\":\"SAI_SWITCH_ASIC_SDK_HEALTH_SEVERITY_FATAL\","
        "\"switch_id\":\"oid:0x21000000000000\","
        "\"timestamp\":\"{"
            "\\\"tv_nsec\\\":\\\"28715881\\\","
            "\\\"tv_sec\\\":\\\"1700042919\\\""
        "}\""
    "}";
    std::vector<swss::FieldValueTuple> asheEntry;
    swss::KeyOpFieldsValuesTuple asheItem(SAI_SWITCH_NOTIFICATION_NAME_SWITCH_ASIC_SDK_HEALTH_EVENT, asheString, asheEntry);
    translator->insertRidAndVid(0x21000000000000,0x210000000000);
    notificationProcessor->syncProcessNotification(asheItem);
    translator->eraseRidAndVid(0x21000000000000,0x210000000000);

    // Test HA_SET_EVENT notification
    std::string haSetEventData = "[{\"event_type\":\"SAI_HA_SET_EVENT_DP_CHANNEL_UP\",\"ha_set_id\":\"oid:0x100000000001\"}]";
    std::vector<swss::FieldValueTuple> haSetEventEntry;
    swss::KeyOpFieldsValuesTuple haSetEventItem(SAI_SWITCH_NOTIFICATION_NAME_HA_SET_EVENT, haSetEventData, haSetEventEntry);
    translator->insertRidAndVid(0x100000000001, 0x100000000001);
    notificationProcessor->syncProcessNotification(haSetEventItem);
    translator->eraseRidAndVid(0x100000000001, 0x100000000001);

    // Test HA_SCOPE_EVENT notification
    std::string haScopeEventData = "[{\"event_type\":\"SAI_HA_SCOPE_EVENT_STATE_CHANGED\",\"ha_scope_id\":\"oid:0x100000000002\",\"ha_role\":\"1\",\"flow_version\":\"100\",\"ha_state\":\"1\"}]";
    std::vector<swss::FieldValueTuple> haScopeEventEntry;
    swss::KeyOpFieldsValuesTuple haScopeEventItem(SAI_SWITCH_NOTIFICATION_NAME_HA_SCOPE_EVENT, haScopeEventData, haScopeEventEntry);
    translator->insertRidAndVid(0x100000000002, 0x100000000002);
    notificationProcessor->syncProcessNotification(haScopeEventItem);
    translator->eraseRidAndVid(0x100000000002, 0x100000000002);

    // Test SWITCH_MACSEC_POST_STATUS notification
    std::string switchPostStatusData = "{\"switch_id\":\"oid:0x21000000000000\",\"macsec_post_status\":\"SAI_SWITCH_MACSEC_POST_STATUS_PASS\"}";
    std::vector<swss::FieldValueTuple> switchPostStatusEntry ;
    swss::KeyOpFieldsValuesTuple switchPostStatusItem(SAI_SWITCH_NOTIFICATION_NAME_SWITCH_MACSEC_POST_STATUS, switchPostStatusData, switchPostStatusEntry);
    translator->insertRidAndVid(0x21000000000000, 0x210000000000);
    notificationProcessor->syncProcessNotification(switchPostStatusItem);
    translator->eraseRidAndVid(0x21000000000000, 0x210000000000);

    // Test MACSEC_POST_STATUS notification
    std::string macsecPostStatusData = "{\"macsec_id\":\"oid:0x5800000000\",\"macsec_post_status\":\"SAI_MACSEC_POST_STATUS_PASS\"}";
    std::vector<swss::FieldValueTuple> macsecPostStatusEntry ;
    swss::KeyOpFieldsValuesTuple macsecPostStatusItem(SAI_SWITCH_NOTIFICATION_NAME_MACSEC_POST_STATUS, macsecPostStatusData, macsecPostStatusEntry);
    translator->insertRidAndVid(0x5800000000, 0x5800000000);
    notificationProcessor->syncProcessNotification(macsecPostStatusItem);
    translator->eraseRidAndVid(0x5800000000, 0x5800000000);

    // Test FLOW_BULK_GET_SESSION_EVENT notification
    std::string flowBulkGetSessionEventData = "{\"bulk_session_id\":\"oid:0x123456789abcdef\",\"data\":[{\"event_type\":\"SAI_FLOW_BULK_GET_SESSION_EVENT_FINISHED\"}]}";
    std::vector<swss::FieldValueTuple> flowBulkGetSessionEventEntry;
    swss::KeyOpFieldsValuesTuple flowBulkGetSessionEventItem(SAI_SWITCH_NOTIFICATION_NAME_FLOW_BULK_GET_SESSION_EVENT, flowBulkGetSessionEventData, flowBulkGetSessionEventEntry);
    translator->insertRidAndVid(0x123456789abcdef, 0x123456789abcdef);
    notificationProcessor->syncProcessNotification(flowBulkGetSessionEventItem);
    translator->eraseRidAndVid(0x123456789abcdef, 0x123456789abcdef);
}


/*
 * Helper: build a processor with a DB connection so m_fdbEventStateProducer
 * is initialized.
 */
static std::shared_ptr<NotificationProcessor> makeProcessorWithDb(
    std::shared_ptr<swss::DBConnector> dbAsic,
    std::shared_ptr<VirtualOidTranslator> &outTranslator)
{
    SWSS_LOG_ENTER();
    auto sai      = std::make_shared<saivs::Sai>();
    auto client   = std::make_shared<RedisClient>(dbAsic);
    auto producer = std::make_shared<syncd::RedisNotificationProducer>("ASIC_DB");

    auto np = std::make_shared<NotificationProcessor>(producer, client,
                                                      [](const swss::KeyOpFieldsValuesTuple&){},
                                                      dbAsic);

    auto switchConfigContainer  = std::make_shared<sairedis::SwitchConfigContainer>();
    auto ridxGen = std::make_shared<sairedis::RedisVidIndexGenerator>(dbAsic, REDIS_KEY_VIDCOUNTER);
    auto voim = std::make_shared<sairedis::VirtualObjectIdManager>(0, switchConfigContainer, ridxGen);

    outTranslator = std::make_shared<VirtualOidTranslator>(client, voim, sai);
    np->m_translator = outTranslator;

    return np;
}

/*
 * ProducerStateTable writes field data to "_FDB_EVENT_STATE:{key}" in Redis.
 * Use this helper to verify what was written.
 */
static std::string fdbStateKey(const std::string &fdbKey)
{
    // SWSS_LOG_ENTER omitted (simple helper)
    return "_FDB_EVENT_STATE:" + fdbKey;
}

/*
 * Test: LEARN event is written to FDB_EVENT_STATE via ProducerStateTable.
 */
TEST(NotificationProcessor, FdbLearnGoesToProducerStateTable)
{
    auto dbAsic = std::make_shared<swss::DBConnector>("ASIC_DB", 0);
    std::shared_ptr<VirtualOidTranslator> translator;
    auto np = makeProcessorWithDb(dbAsic, translator);

    translator->insertRidAndVid(0x21000000000000, 0x210000000000);
    translator->insertRidAndVid(0x1003a0000004c,  0x3a000000000c00);
    translator->insertRidAndVid(0x2600000010,     0x26000000000010);

    std::string fdbKey =
        "{\"bvid\":\"oid:0x26000000000010\","
        "\"mac\":\"00:00:00:00:00:10\","
        "\"switch_id\":\"oid:0x210000000000\"}";

    dbAsic->del(fdbStateKey(fdbKey));

    std::string learnData =
        "[{\"fdb_entry\":\"{\\\"bvid\\\":\\\"oid:0x2600000010\\\","
        "\\\"mac\\\":\\\"00:00:00:00:00:10\\\","
        "\\\"switch_id\\\":\\\"oid:0x21000000000000\\\"}\","
        "\"fdb_event\":\"SAI_FDB_EVENT_LEARNED\","
        "\"list\":["
            "{\"id\":\"SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID\","
             "\"value\":\"oid:0x1003a0000004c\"},"
            "{\"id\":\"SAI_FDB_ENTRY_ATTR_TYPE\","
             "\"value\":\"SAI_FDB_ENTRY_TYPE_DYNAMIC\"}"
        "]}]";

    std::vector<swss::FieldValueTuple> empty;
    swss::KeyOpFieldsValuesTuple item(SAI_SWITCH_NOTIFICATION_NAME_FDB_EVENT, learnData, empty);
    np->syncProcessNotification(item);

    auto val = dbAsic->hget(fdbStateKey(fdbKey), "event_type");
    EXPECT_NE(val, nullptr) << "LEARN event not written to FDB_EVENT_STATE";
    if (val)
    {
        EXPECT_EQ(*val, "SAI_FDB_EVENT_LEARNED");
    }

    dbAsic->del(fdbStateKey(fdbKey));
    dbAsic->del("FDB_EVENT_STATE_KEY_SET");
    translator->eraseRidAndVid(0x21000000000000, 0x210000000000);
    translator->eraseRidAndVid(0x1003a0000004c,  0x3a000000000c00);
    translator->eraseRidAndVid(0x2600000010,     0x26000000000010);
}

/*
 * Test: AGED event issues DEL on FDB_EVENT_STATE (deletes the key).
 */
TEST(NotificationProcessor, FdbAgedDeletesFromProducerStateTable)
{
    auto dbAsic = std::make_shared<swss::DBConnector>("ASIC_DB", 0);
    std::shared_ptr<VirtualOidTranslator> translator;
    auto np = makeProcessorWithDb(dbAsic, translator);

    translator->insertRidAndVid(0x21000000000000, 0x210000000000);
    translator->insertRidAndVid(0x2600000011,     0x26000000000011);

    std::string fdbKey =
        "{\"bvid\":\"oid:0x26000000000011\","
        "\"mac\":\"00:00:00:00:00:11\","
        "\"switch_id\":\"oid:0x210000000000\"}";

    /* Pre-populate so we can verify deletion */
    dbAsic->hset(fdbStateKey(fdbKey), "event_type", "SAI_FDB_EVENT_LEARNED");

    std::string agedData =
        "[{\"fdb_entry\":\"{\\\"bvid\\\":\\\"oid:0x2600000011\\\","
        "\\\"mac\\\":\\\"00:00:00:00:00:11\\\","
        "\\\"switch_id\\\":\\\"oid:0x21000000000000\\\"}\","
        "\"fdb_event\":\"SAI_FDB_EVENT_AGED\","
        "\"list\":[]}]";

    std::vector<swss::FieldValueTuple> empty;
    swss::KeyOpFieldsValuesTuple item(SAI_SWITCH_NOTIFICATION_NAME_FDB_EVENT, agedData, empty);
    np->syncProcessNotification(item);

    /* ProducerStateTable::del does not remove the key immediately in Redis;
     * it publishes a DEL notification. The _G_ scratch key should be deleted. */
    auto val = dbAsic->hget(fdbStateKey(fdbKey), "event_type");
    EXPECT_EQ(val, nullptr) << "AGED event should delete entry from FDB_EVENT_STATE scratch";

    dbAsic->del("FDB_EVENT_STATE_KEY_SET");
    translator->eraseRidAndVid(0x21000000000000, 0x210000000000);
    translator->eraseRidAndVid(0x2600000011,     0x26000000000011);
}

/*
 * Test: FLUSH event goes via PUBLISH, not to FDB_EVENT_STATE.
 */
TEST(NotificationProcessor, FdbFlushDoesNotWriteToProducerStateTable)
{
    auto dbAsic = std::make_shared<swss::DBConnector>("ASIC_DB", 0);
    std::shared_ptr<VirtualOidTranslator> translator;
    auto np = makeProcessorWithDb(dbAsic, translator);

    translator->insertRidAndVid(0x21000000000000, 0x210000000000);
    translator->insertRidAndVid(0x2600000012,     0x26000000000012);

    std::string fdbKey =
        "{\"bvid\":\"oid:0x26000000000012\","
        "\"mac\":\"00:00:00:00:00:00\","
        "\"switch_id\":\"oid:0x210000000000\"}";

    dbAsic->del(fdbStateKey(fdbKey));

    std::string flushData =
        "[{\"fdb_entry\":\"{\\\"bvid\\\":\\\"oid:0x2600000012\\\","
        "\\\"mac\\\":\\\"00:00:00:00:00:00\\\","
        "\\\"switch_id\\\":\\\"oid:0x21000000000000\\\"}\","
        "\"fdb_event\":\"SAI_FDB_EVENT_FLUSHED\","
        "\"list\":[]}]";

    std::vector<swss::FieldValueTuple> empty;
    swss::KeyOpFieldsValuesTuple item(SAI_SWITCH_NOTIFICATION_NAME_FDB_EVENT, flushData, empty);
    np->syncProcessNotification(item);

    /* FLUSH must NOT appear in the scratch table */
    auto val = dbAsic->hget(fdbStateKey(fdbKey), "event_type");
    EXPECT_EQ(val, nullptr) << "FLUSH event must not be written to FDB_EVENT_STATE";

    dbAsic->del("FDB_EVENT_STATE_KEY_SET");
    translator->eraseRidAndVid(0x21000000000000, 0x210000000000);
    translator->eraseRidAndVid(0x2600000012,     0x26000000000012);
}
