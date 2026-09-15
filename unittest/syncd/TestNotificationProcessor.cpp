#include "VirtualOidTranslator.h"
#include "RedisClient.h"
#include "RedisNotificationProducer.h"
#include "NotificationProcessor.h"
#include "lib/RedisVidIndexGenerator.h"
#include "lib/sairediscommon.h"
#include "vslib/Sai.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <mutex>
#include <vector>
#include <string>

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

// ---------------------------------------------------------------------------
// Lost-wakeup / wait-predicate tests for the NotificationProcessor threading.
//
// ntf_process_function() now waits on a predicate (queue non-empty || !running)
// under the member mutex m_mtx, and signal()/stop take that same mutex around
// notify_all(). The old predicate-less m_cv.wait() could miss a wakeup that
// raced ahead of the wait, leaving the tail of a burst stuck in the queue.
//
// processNotification(NotificationItem) forwards a normal notification to
// processNotification(item.notification) -> the constructor-supplied
// synchronizer, so a counting synchronizer is the observation point: no SAI
// deserialize / redis / translator is needed, and producer & client (only
// stored on this path) can be nullptr.
// ---------------------------------------------------------------------------

namespace
{
    // A notification whose op is NOT the flow-bulk event, so it routes through
    // processNotification(item.notification) -> synchronizer. The key doubles
    // as a payload marker.
    swss::KeyOpFieldsValuesTuple makeNtf(const std::string& key)
    {
        std::vector<swss::FieldValueTuple> values;
        return swss::KeyOpFieldsValuesTuple(key, "test-op", values);
    }

    bool waitUntil(const std::function<bool()>& pred, int timeoutMs = 5000)
    {
        auto deadline = std::chrono::steady_clock::now()
                        + std::chrono::milliseconds(timeoutMs);
        while (std::chrono::steady_clock::now() < deadline)
        {
            if (pred())
            {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return pred();
    }

    struct Sink
    {
        std::atomic<int> processed{0};
        std::mutex mtx;
        std::vector<std::string> keys;

        std::function<void(const swss::KeyOpFieldsValuesTuple&)> fn()
        {
            return [this](const swss::KeyOpFieldsValuesTuple& item)
            {
                {
                    std::lock_guard<std::mutex> l(mtx);
                    keys.push_back(kfvKey(item));
                }
                processed++;
            };
        }
    };
}

// Core regression: an item enqueued BEFORE the thread starts and with NO
// signal() must still be processed, because the first predicate check sees
// queueSize > 0. With the old predicate-less wait() the thread blocks forever
// here and the test times out.
TEST(NotificationProcessor, LostWakeup_EnqueueBeforeStart_NoSignal)
{
    Sink sink;
    auto np = std::make_shared<NotificationProcessor>(nullptr, nullptr, sink.fn());

    np->getQueue()->enqueue(makeNtf("evt-0"));      // queue non-empty first
    np->startNotificationsProcessingThread();       // ... then start, no signal()

    EXPECT_TRUE(waitUntil([&]{ return sink.processed.load() == 1; }));

    np->stopNotificationsProcessingThread();
    EXPECT_EQ(sink.processed.load(), 1);
    EXPECT_EQ(np->getQueue()->getQueueSize(), 0u);  // nothing left stuck
}

// Producer hammers enqueue()+signal() concurrently with the consumer's
// wait/drain cycle. Every enqueued item must be processed exactly once.
TEST(NotificationProcessor, LostWakeup_ConcurrentEnqueueSignal_NoneLost)
{
    const int N = 500;
    Sink sink;
    auto np = std::make_shared<NotificationProcessor>(nullptr, nullptr, sink.fn());

    np->startNotificationsProcessingThread();

    std::thread producer([&]()
    {
        for (int i = 0; i < N; ++i)
        {
            np->getQueue()->enqueue(makeNtf("evt-" + std::to_string(i)));
            np->signal();                            // race against wait/drain
        }
    });
    producer.join();

    EXPECT_TRUE(waitUntil([&]{ return sink.processed.load() == N; }));

    np->stopNotificationsProcessingThread();
    EXPECT_EQ(sink.processed.load(), N);
    EXPECT_EQ(np->getQueue()->getQueueSize(), 0u);

    std::lock_guard<std::mutex> l(sink.mtx);
    EXPECT_EQ((int)sink.keys.size(), N);             // no duplicates / no loss
}

// A whole burst enqueued, then a SINGLE signal(): the drain loop plus the
// non-empty predicate must process every item from that one wakeup, even if
// the signal races ahead of the wait.
TEST(NotificationProcessor, BurstThenSingleSignal_AllDrained)
{
    const int M = 1000;
    Sink sink;
    auto np = std::make_shared<NotificationProcessor>(nullptr, nullptr, sink.fn());

    np->startNotificationsProcessingThread();

    for (int i = 0; i < M; ++i)
    {
        np->getQueue()->enqueue(makeNtf("burst-" + std::to_string(i)));
    }
    np->signal();                                    // one signal for the burst

    EXPECT_TRUE(waitUntil([&]{ return sink.processed.load() == M; }));

    np->stopNotificationsProcessingThread();
    EXPECT_EQ(sink.processed.load(), M);
    EXPECT_EQ(np->getQueue()->getQueueSize(), 0u);
}

// The !m_runThread branch of the predicate: a thread blocked on an empty queue
// must wake and exit promptly on stop (stop must not hang).
TEST(NotificationProcessor, ShutdownWakesWaitingThread)
{
    Sink sink;
    auto np = std::make_shared<NotificationProcessor>(nullptr, nullptr, sink.fn());

    np->startNotificationsProcessingThread();
    std::this_thread::sleep_for(std::chrono::milliseconds(50)); // let it block in wait()

    auto t0 = std::chrono::steady_clock::now();
    np->stopNotificationsProcessingThread();                    // joins the thread
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::steady_clock::now() - t0).count();

    EXPECT_LT(elapsedMs, 1000);                      // woke via predicate, no hang
    EXPECT_EQ(sink.processed.load(), 0);
}
