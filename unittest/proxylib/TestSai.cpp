#include <cstdint>

#include <memory>
#include <vector>
#include <array>

#include <gtest/gtest.h>

#include "meta/DummySaiInterface.h"

#include "Sai.h"
#include "Proxy.h"

// TODO fix join when tests will fail

using namespace saiproxy;

class SaiTest : public ::testing::Test
{
public:
    SaiTest() = default;
    virtual ~SaiTest() = default;

public:
    virtual void SetUp() override
    {
        m_sai = std::make_shared<Sai>();

        //sai_attribute_t attr;
        //attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
        //attr.value.booldata = true;

        //auto status = m_sai->create(SAI_OBJECT_TYPE_SWITCH, &m_swid, SAI_NULL_OBJECT_ID, 1, &attr);
        //ASSERT_EQ(status, SAI_STATUS_SUCCESS);
    }

    virtual void TearDown() override
    {
        //auto status = m_sai->remove(SAI_OBJECT_TYPE_SWITCH, m_swid);
        //ASSERT_EQ(status, SAI_STATUS_SUCCESS);
    }

protected:
    std::shared_ptr<Sai> m_sai;

    sai_object_id_t m_swid = SAI_NULL_OBJECT_ID;

    const std::uint32_t m_guid = 0; // default context config id
    const std::uint32_t m_scid = 0; // default switch config id
};

static const char* profile_get_value(
        _In_ sai_switch_profile_id_t profile_id,
        _In_ const char* variable)
{
    SWSS_LOG_ENTER();

    if (variable == NULL)
        return NULL;

    return nullptr;
}

static int profile_get_next_value(
        _In_ sai_switch_profile_id_t profile_id,
        _Out_ const char** variable,
        _Out_ const char** value)
{
    SWSS_LOG_ENTER();

    return 0;
}

static sai_service_method_table_t test_services = {
    profile_get_value,
    profile_get_next_value
};

TEST_F(SaiTest, Ctr)
{
    auto s = std::make_shared<Sai>();
}

TEST(Sai, bulkGet)
{
    Sai sai;

    sai_object_id_t oids[1] = {0};
    uint32_t attrcount[1] = {0};
    sai_attribute_t* attrs[1] = {0};
    sai_status_t statuses[1] = {0};


    EXPECT_EQ(SAI_STATUS_NOT_IMPLEMENTED,
            sai.bulkGet(
                SAI_OBJECT_TYPE_PORT,
                1,
                oids,
                attrcount,
                attrs,
                SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR,
                statuses));
}

TEST(Sai, apiInitialize)
{
    Sai sai;

    // non zero flags
    EXPECT_EQ(sai.apiInitialize(1, &test_services), SAI_STATUS_INVALID_PARAMETER);

    // table null
    EXPECT_EQ(sai.apiInitialize(0, NULL), SAI_STATUS_INVALID_PARAMETER);

    // correct one
    EXPECT_EQ(sai.apiInitialize(0, &test_services), SAI_STATUS_SUCCESS);

    // second initialize
    EXPECT_EQ(sai.apiInitialize(0, &test_services), SAI_STATUS_FAILURE);
}

TEST(Sai, apiUninitialize)
{
    Sai sai;

    EXPECT_EQ(sai.apiInitialize(0, &test_services), SAI_STATUS_SUCCESS);

    EXPECT_EQ(sai.apiUninitialize(), SAI_STATUS_SUCCESS);
}

static void fun(std::shared_ptr<Proxy> proxy)
{
    SWSS_LOG_ENTER();

    proxy->run();
}

TEST(Sai, create)
{
    Sai sai;

    EXPECT_EQ(sai.apiInitialize(0, &test_services), SAI_STATUS_SUCCESS);

    std::shared_ptr<sairedis::SaiInterface> dummy = std::make_shared<saimeta::DummySaiInterface>();

    auto proxy = std::make_shared<Proxy>(dummy);

    auto thread = std::make_shared<std::thread>(fun,proxy);

    sai_object_id_t switch_id = SAI_NULL_OBJECT_ID;

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    // create oid
    auto status = sai.create(
            SAI_OBJECT_TYPE_SWITCH,
            &switch_id,
            SAI_NULL_OBJECT_ID, // creating switch
            1,
            &attr);

    EXPECT_EQ(status, SAI_STATUS_SUCCESS);
    EXPECT_NE(switch_id, SAI_NULL_OBJECT_ID);

    sai_fdb_entry_t fdb = {};

    attr.id = SAI_FDB_ENTRY_ATTR_META_DATA;
    attr.value.u32 = 0;

    // create entry
    status = sai.create(&fdb, 1, &attr);

    EXPECT_EQ(status, SAI_STATUS_SUCCESS);

    proxy->stop();

    thread->join();
}

TEST(Sai, remove)
{
    Sai sai;

    EXPECT_EQ(sai.apiInitialize(0, &test_services), SAI_STATUS_SUCCESS);

    std::shared_ptr<sairedis::SaiInterface> dummy = std::make_shared<saimeta::DummySaiInterface>();

    auto proxy = std::make_shared<Proxy>(dummy);

    auto thread = std::make_shared<std::thread>(fun,proxy);

    // remove oid
    auto status = sai.remove(
            SAI_OBJECT_TYPE_SWITCH,
            (sai_object_id_t)1);

    EXPECT_EQ(status, SAI_STATUS_SUCCESS);

    sai_fdb_entry_t fdb = {};

    // remove entry
    status = sai.remove(&fdb);

    EXPECT_EQ(status, SAI_STATUS_SUCCESS);

    proxy->stop();

    thread->join();
}

TEST(Sai, set)
{
    Sai sai;

    EXPECT_EQ(sai.apiInitialize(0, &test_services), SAI_STATUS_SUCCESS);

    std::shared_ptr<sairedis::SaiInterface> dummy = std::make_shared<saimeta::DummySaiInterface>();

    auto proxy = std::make_shared<Proxy>(dummy);

    auto thread = std::make_shared<std::thread>(fun,proxy);

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    // set oid
    auto status = sai.set(
            SAI_OBJECT_TYPE_SWITCH,
            (sai_object_id_t)1,
            &attr);

    EXPECT_EQ(status, SAI_STATUS_SUCCESS);

    sai_fdb_entry_t fdb = {};

    attr.id = SAI_FDB_ENTRY_ATTR_META_DATA;
    attr.value.u32 = 0;

    // set entry
    status = sai.set(&fdb, &attr);

    EXPECT_EQ(status, SAI_STATUS_SUCCESS);

    proxy->stop();

    thread->join();
}

TEST(Sai, get)
{
    Sai sai;

    EXPECT_EQ(sai.apiInitialize(0, &test_services), SAI_STATUS_SUCCESS);

    std::shared_ptr<sairedis::SaiInterface> dummy = std::make_shared<saimeta::DummySaiInterface>();

    auto proxy = std::make_shared<Proxy>(dummy);

    auto thread = std::make_shared<std::thread>(fun,proxy);

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;

    // get oid
    auto status = sai.get(
            SAI_OBJECT_TYPE_SWITCH,
            (sai_object_id_t)1,
            1,
            &attr);

    EXPECT_EQ(status, SAI_STATUS_SUCCESS);

    sai_fdb_entry_t fdb = {};

    attr.id = SAI_FDB_ENTRY_ATTR_META_DATA;

    // get entry
    status = sai.get(&fdb, 1, &attr);

    EXPECT_EQ(status, SAI_STATUS_SUCCESS);

    proxy->stop();

    thread->join();
}

TEST(Sai, flushFdbEntries)
{
    Sai sai;

    EXPECT_EQ(sai.apiInitialize(0, &test_services), SAI_STATUS_SUCCESS);

    std::shared_ptr<sairedis::SaiInterface> dummy = std::make_shared<saimeta::DummySaiInterface>();

    auto proxy = std::make_shared<Proxy>(dummy);

    auto thread = std::make_shared<std::thread>(fun,proxy);

    auto status = sai.flushFdbEntries((sai_object_id_t)1, 0, 0);

    EXPECT_EQ(status, SAI_STATUS_SUCCESS);

    proxy->stop();

    thread->join();
}


TEST(Sai, processObjectTypeGetAvailability)
{
    Sai sai;

    EXPECT_EQ(sai.apiInitialize(0, &test_services), SAI_STATUS_SUCCESS);

    std::shared_ptr<sairedis::SaiInterface> dummy = std::make_shared<saimeta::DummySaiInterface>();

    auto proxy = std::make_shared<Proxy>(dummy);

    auto thread = std::make_shared<std::thread>(fun,proxy);

    uint64_t count;

    auto status = sai.objectTypeGetAvailability(
            (sai_object_id_t)1,
            SAI_OBJECT_TYPE_SWITCH,
            0,
            0,
            &count);


    EXPECT_EQ(status, SAI_STATUS_SUCCESS);

    proxy->stop();

    thread->join();
}

TEST(Sai, processQueryAttributeCapability)
{
    Sai sai;

    EXPECT_EQ(sai.apiInitialize(0, &test_services), SAI_STATUS_SUCCESS);

    std::shared_ptr<sairedis::SaiInterface> dummy = std::make_shared<saimeta::DummySaiInterface>();

    auto proxy = std::make_shared<Proxy>(dummy);

    auto thread = std::make_shared<std::thread>(fun,proxy);

    sai_attr_capability_t cap;

    auto status = sai.queryAttributeCapability(
            (sai_object_id_t)1,
            SAI_OBJECT_TYPE_SWITCH,
            SAI_SWITCH_ATTR_INIT_SWITCH,
            &cap);

    EXPECT_EQ(status, SAI_STATUS_SUCCESS);

    proxy->stop();

    thread->join();
}

TEST(Sai, queryAttributeEnumValuesCapability)
{
    Sai sai;

    EXPECT_EQ(sai.apiInitialize(0, &test_services), SAI_STATUS_SUCCESS);

    std::shared_ptr<sairedis::SaiInterface> dummy = std::make_shared<saimeta::DummySaiInterface>();

    auto proxy = std::make_shared<Proxy>(dummy);

    auto thread = std::make_shared<std::thread>(fun,proxy);

    sai_s32_list_t list;

    int32_t arr[10];

    list.count = 10;
    list.list = arr;

    auto status = sai.queryAttributeEnumValuesCapability(
            (sai_object_id_t)1,
            SAI_OBJECT_TYPE_SWITCH,
            SAI_SWITCH_ATTR_INIT_SWITCH,
            &list);

    EXPECT_EQ(status, SAI_STATUS_SUCCESS);

    proxy->stop();

    thread->join();
}

TEST(Sai, objectTypeQuery)
{
    Sai sai;

    EXPECT_EQ(sai.apiInitialize(0, &test_services), SAI_STATUS_SUCCESS);

    std::shared_ptr<sairedis::SaiInterface> dummy = std::make_shared<saimeta::DummySaiInterface>();

    auto proxy = std::make_shared<Proxy>(dummy);

    auto thread = std::make_shared<std::thread>(fun,proxy);

    auto objectType = sai.objectTypeQuery(SAI_NULL_OBJECT_ID);

    EXPECT_EQ(objectType, SAI_OBJECT_TYPE_NULL);

    proxy->stop();

    thread->join();
}

TEST(Sai, switchIdQuery)
{
    Sai sai;

    EXPECT_EQ(sai.apiInitialize(0, &test_services), SAI_STATUS_SUCCESS);

    std::shared_ptr<sairedis::SaiInterface> dummy = std::make_shared<saimeta::DummySaiInterface>();

    auto proxy = std::make_shared<Proxy>(dummy);

    auto thread = std::make_shared<std::thread>(fun,proxy);

    auto objectId = sai.switchIdQuery(SAI_NULL_OBJECT_ID);

    EXPECT_EQ(objectId, SAI_NULL_OBJECT_ID);

    proxy->stop();

    thread->join();
}

TEST(Sai, queryApiVersion)
{
    Sai sai;

    sai_api_version_t version;

    // api not initialized
    EXPECT_EQ(sai.queryApiVersion(&version), SAI_STATUS_FAILURE);

    EXPECT_EQ(sai.apiInitialize(0, &test_services), SAI_STATUS_SUCCESS);

    std::shared_ptr<sairedis::SaiInterface> dummy = std::make_shared<saimeta::DummySaiInterface>();

    auto proxy = std::make_shared<Proxy>(dummy);

    auto thread = std::make_shared<std::thread>(fun,proxy);

    EXPECT_EQ(sai.queryApiVersion(NULL), SAI_STATUS_INVALID_PARAMETER);

    auto status = sai.queryApiVersion(&version);

    EXPECT_EQ(status, SAI_STATUS_SUCCESS);
    EXPECT_EQ(version, SAI_API_VERSION);

    proxy->stop();

    thread->join();
}

TEST(Sai, logSet)
{
    Sai sai;

    sai_api_version_t version;

    // api not initialized
    EXPECT_EQ(sai.queryApiVersion(&version), SAI_STATUS_FAILURE);

    EXPECT_EQ(sai.apiInitialize(0, &test_services), SAI_STATUS_SUCCESS);

    std::shared_ptr<sairedis::SaiInterface> dummy = std::make_shared<saimeta::DummySaiInterface>();

    auto proxy = std::make_shared<Proxy>(dummy);

    auto thread = std::make_shared<std::thread>(fun,proxy);

    auto status = sai.logSet(SAI_API_SWITCH, SAI_LOG_LEVEL_NOTICE);

    EXPECT_EQ(status, SAI_STATUS_SUCCESS);

    proxy->stop();

    thread->join();
}

TEST(Sai, getStats)
{
    Sai sai;

    sai_api_version_t version;

    // api not initialized
    EXPECT_EQ(sai.queryApiVersion(&version), SAI_STATUS_FAILURE);

    EXPECT_EQ(sai.apiInitialize(0, &test_services), SAI_STATUS_SUCCESS);

    std::shared_ptr<sairedis::SaiInterface> dummy = std::make_shared<saimeta::DummySaiInterface>();

    auto proxy = std::make_shared<Proxy>(dummy);

    auto thread = std::make_shared<std::thread>(fun,proxy);

    sai_stat_id_t counter_ids[2] = { SAI_PORT_STAT_IF_IN_OCTETS, SAI_PORT_STAT_IF_IN_UCAST_PKTS };

    uint64_t counters[2];

    auto status = sai.getStats(
            SAI_OBJECT_TYPE_PORT,
            (sai_object_id_t)1,
            2,
            counter_ids,
            counters);

    EXPECT_EQ(status, SAI_STATUS_SUCCESS);

    proxy->stop();

    thread->join();
}

TEST(Sai, getStatsExt)
{
    Sai sai;

    sai_api_version_t version;

    // api not initialized
    EXPECT_EQ(sai.queryApiVersion(&version), SAI_STATUS_FAILURE);

    EXPECT_EQ(sai.apiInitialize(0, &test_services), SAI_STATUS_SUCCESS);

    std::shared_ptr<sairedis::SaiInterface> dummy = std::make_shared<saimeta::DummySaiInterface>();

    auto proxy = std::make_shared<Proxy>(dummy);

    auto thread = std::make_shared<std::thread>(fun,proxy);

    sai_stat_id_t counter_ids[2] = { SAI_PORT_STAT_IF_IN_OCTETS, SAI_PORT_STAT_IF_IN_UCAST_PKTS };

    uint64_t counters[2];

    auto status = sai.getStatsExt(
            SAI_OBJECT_TYPE_PORT,
            (sai_object_id_t)1,
            2,
            counter_ids,
            SAI_STATS_MODE_READ,
            counters);

    EXPECT_EQ(status, SAI_STATUS_SUCCESS);

    proxy->stop();

    thread->join();
}

TEST(Sai, clearStats)
{
    Sai sai;

    sai_api_version_t version;

    // api not initialized
    EXPECT_EQ(sai.queryApiVersion(&version), SAI_STATUS_FAILURE);

    EXPECT_EQ(sai.apiInitialize(0, &test_services), SAI_STATUS_SUCCESS);

    std::shared_ptr<sairedis::SaiInterface> dummy = std::make_shared<saimeta::DummySaiInterface>();

    auto proxy = std::make_shared<Proxy>(dummy);

    auto thread = std::make_shared<std::thread>(fun,proxy);

    sai_stat_id_t counter_ids[2] = { SAI_PORT_STAT_IF_IN_OCTETS, SAI_PORT_STAT_IF_IN_UCAST_PKTS };

    auto status = sai.clearStats(
            SAI_OBJECT_TYPE_PORT,
            (sai_object_id_t)1,
            2,
            counter_ids);

    EXPECT_EQ(status, SAI_STATUS_SUCCESS);

    proxy->stop();

    thread->join();
}

static int ntfCounter = 0;

static void onSwitchStateChange(
        _In_ sai_object_id_t switch_id,
        _In_ sai_switch_oper_status_t switch_oper_status)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("received onSwitchStateChange");

    ntfCounter++;
}

static void onFdbEvent(
        _In_ uint32_t count,
        _In_ const sai_fdb_event_notification_data_t *data)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("received onFdbEvent");

    ntfCounter++;
}

static void onPortStateChange(
        _In_ uint32_t count,
        _In_ const sai_port_oper_status_notification_t *data)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("received onPortStateChange");

    ntfCounter++;
}

static void onSwitchShutdownRequest(
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("received: onSwitchShutdownRequest");

    ntfCounter++;
}

static void onNatEvent(
        _In_ uint32_t count,
        _In_ const sai_nat_event_notification_data_t *data)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("received: onNatEvent");

    ntfCounter++;
}

static void onPortHostTxReady(
        _In_ sai_object_id_t switch_id,
        _In_ sai_object_id_t port_id,
        _In_ sai_port_host_tx_ready_status_t host_tx_ready_status)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("received: onPortHostTxReady");

    ntfCounter++;
}

static void onQueuePfcDeadlock(
        _In_ uint32_t count,
        _In_ const sai_queue_deadlock_notification_data_t *data)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("received: onQueuePfcDeadlock");

    ntfCounter++;
}

static void onSwitchAsicSdkHealthEvent(
        _In_ sai_object_id_t switch_id,
        _In_ sai_switch_asic_sdk_health_severity_t severity,
        _In_ sai_timespec_t timestamp,
        _In_ sai_switch_asic_sdk_health_category_t category,
        _In_ sai_switch_health_data_t data,
        _In_ const sai_u8_list_t description)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("received: onSwitchAsicSdkHealthEvent");

    ntfCounter++;
}

static void onBfdSessionStateChange(
        _In_ uint32_t count,
        _In_ const sai_bfd_session_state_notification_t *data)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("received: onBfdSessionStateChange");

    ntfCounter++;
}

static void onTwampSessionEvent(
        _In_ uint32_t count,
        _In_ const sai_twamp_session_event_notification_data_t *data)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("received: onTwampSessionEvent");

    ntfCounter++;
}

static void onTamTelTypeConfigChange(
        _In_ sai_object_id_t tam_tel_id)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("received: onTamTelTypeConfigChange");

    ntfCounter++;
}

TEST(Sai, handleNotification)
{
    Sai sai;

    EXPECT_EQ(sai.apiInitialize(0, &test_services), SAI_STATUS_SUCCESS);

    std::shared_ptr<saimeta::DummySaiInterface> dummy = std::make_shared<saimeta::DummySaiInterface>();

    auto proxy = std::make_shared<Proxy>(dummy);

    EXPECT_EQ(dummy->enqueueNotificationToSend(SAI_SWITCH_ATTR_SWITCH_STATE_CHANGE_NOTIFY), SAI_STATUS_SUCCESS);
    EXPECT_EQ(dummy->enqueueNotificationToSend(SAI_SWITCH_ATTR_FDB_EVENT_NOTIFY), SAI_STATUS_SUCCESS);
    EXPECT_EQ(dummy->enqueueNotificationToSend(SAI_SWITCH_ATTR_PORT_STATE_CHANGE_NOTIFY), SAI_STATUS_SUCCESS);
    EXPECT_EQ(dummy->enqueueNotificationToSend(SAI_SWITCH_ATTR_SHUTDOWN_REQUEST_NOTIFY), SAI_STATUS_SUCCESS);
    EXPECT_EQ(dummy->enqueueNotificationToSend(SAI_SWITCH_ATTR_SWITCH_ASIC_SDK_HEALTH_EVENT_NOTIFY), SAI_STATUS_SUCCESS);
    EXPECT_EQ(dummy->enqueueNotificationToSend(SAI_SWITCH_ATTR_NAT_EVENT_NOTIFY), SAI_STATUS_SUCCESS);
    EXPECT_EQ(dummy->enqueueNotificationToSend(SAI_SWITCH_ATTR_PORT_HOST_TX_READY_NOTIFY), SAI_STATUS_SUCCESS);
    EXPECT_EQ(dummy->enqueueNotificationToSend(SAI_SWITCH_ATTR_QUEUE_PFC_DEADLOCK_NOTIFY), SAI_STATUS_SUCCESS);
    EXPECT_EQ(dummy->enqueueNotificationToSend(SAI_SWITCH_ATTR_BFD_SESSION_STATE_CHANGE_NOTIFY), SAI_STATUS_SUCCESS);
    EXPECT_EQ(dummy->enqueueNotificationToSend(SAI_SWITCH_ATTR_TWAMP_SESSION_EVENT_NOTIFY), SAI_STATUS_SUCCESS);
    EXPECT_EQ(dummy->enqueueNotificationToSend(SAI_SWITCH_ATTR_TAM_TEL_TYPE_CONFIG_CHANGE_NOTIFY), SAI_STATUS_SUCCESS);

    auto thread = std::make_shared<std::thread>(fun, proxy);

    sai_object_id_t switch_id;

    sai_attribute_t attr;

    attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attr.value.booldata = true;

    // create oid
    auto status = sai.create(
            SAI_OBJECT_TYPE_SWITCH,
            &switch_id,
            SAI_NULL_OBJECT_ID, // creating switch
            1,
            &attr);

    EXPECT_EQ(status, SAI_STATUS_SUCCESS);
    EXPECT_NE(switch_id, SAI_NULL_OBJECT_ID);

    // set notification pointer

    attr.id = SAI_SWITCH_ATTR_SWITCH_STATE_CHANGE_NOTIFY;
    attr.value.ptr = (void*)&onSwitchStateChange;
    EXPECT_EQ(sai.set(SAI_OBJECT_TYPE_SWITCH, switch_id, &attr), SAI_STATUS_SUCCESS);

    attr.id = SAI_SWITCH_ATTR_FDB_EVENT_NOTIFY;
    attr.value.ptr = (void*)&onFdbEvent;
    EXPECT_EQ(sai.set(SAI_OBJECT_TYPE_SWITCH, switch_id, &attr), SAI_STATUS_SUCCESS);

    attr.id = SAI_SWITCH_ATTR_PORT_STATE_CHANGE_NOTIFY;
    attr.value.ptr = (void*)&onPortStateChange;
    EXPECT_EQ(sai.set(SAI_OBJECT_TYPE_SWITCH, switch_id, &attr), SAI_STATUS_SUCCESS);

    attr.id = SAI_SWITCH_ATTR_SHUTDOWN_REQUEST_NOTIFY;
    attr.value.ptr = (void*)&onSwitchShutdownRequest;
    EXPECT_EQ(sai.set(SAI_OBJECT_TYPE_SWITCH, switch_id, &attr), SAI_STATUS_SUCCESS);

    attr.id = SAI_SWITCH_ATTR_SWITCH_ASIC_SDK_HEALTH_EVENT_NOTIFY;
    attr.value.ptr = (void*)&onSwitchAsicSdkHealthEvent;
    sai.set(SAI_OBJECT_TYPE_SWITCH, switch_id, &attr);

    attr.id = SAI_SWITCH_ATTR_NAT_EVENT_NOTIFY;
    attr.value.ptr = (void*)&onNatEvent;
    sai.set(SAI_OBJECT_TYPE_SWITCH, switch_id, &attr);

    attr.id = SAI_SWITCH_ATTR_PORT_HOST_TX_READY_NOTIFY;
    attr.value.ptr = (void*)&onPortHostTxReady;
    sai.set(SAI_OBJECT_TYPE_SWITCH, switch_id, &attr);

    attr.id = SAI_SWITCH_ATTR_QUEUE_PFC_DEADLOCK_NOTIFY;
    attr.value.ptr = (void*)&onQueuePfcDeadlock;
    sai.set(SAI_OBJECT_TYPE_SWITCH, switch_id, &attr);

    attr.id = SAI_SWITCH_ATTR_BFD_SESSION_STATE_CHANGE_NOTIFY;
    attr.value.ptr = (void*)&onBfdSessionStateChange;
    sai.set(SAI_OBJECT_TYPE_SWITCH, switch_id, &attr);

    attr.id = SAI_SWITCH_ATTR_TWAMP_SESSION_EVENT_NOTIFY;
    attr.value.ptr = (void*)&onTwampSessionEvent;
    sai.set(SAI_OBJECT_TYPE_SWITCH, switch_id, &attr);

    attr.id = SAI_SWITCH_ATTR_TAM_TEL_TYPE_CONFIG_CHANGE_NOTIFY;
    attr.value.ptr = (void*)&onTamTelTypeConfigChange;
    sai.set(SAI_OBJECT_TYPE_SWITCH, switch_id, &attr);

    // dummy start sending notifications
    EXPECT_EQ(dummy->start(), SAI_STATUS_SUCCESS);

    sleep(1); // give some time for proxy to receive notification

    // dummy stop sending notifications
    EXPECT_EQ(dummy->stop(), SAI_STATUS_SUCCESS);

    EXPECT_EQ(proxy->getNotificationsSentCount(), 11);

    // important check, whether Sai class processed notifications correctly
    EXPECT_EQ(ntfCounter, 11);

    proxy->stop();

    thread->join();
}

