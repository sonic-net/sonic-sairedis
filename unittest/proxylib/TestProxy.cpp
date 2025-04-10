#include <cstdint>

#include <memory>
#include <vector>
#include <array>

#include <gtest/gtest.h>

#include "meta/DummySaiInterface.h"

#include "Sai.h"
#include "Proxy.h"

using namespace saiproxy;

TEST(Proxy, ctr)
{
    Sai sai;

    std::shared_ptr<sairedis::SaiInterface> dummy = std::make_shared<saimeta::DummySaiInterface>();

    // will test loadProfileMap

    auto proxy = std::make_shared<Proxy>(dummy);
}

static void fun(std::shared_ptr<Proxy> proxy)
{
    SWSS_LOG_ENTER();

    proxy->run();
}

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

TEST(Proxy, notifications)
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

    auto thread = std::make_shared<std::thread>(fun,proxy);

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

    EXPECT_EQ(proxy->getNotificationsSentCount(), 4+6+1);

    proxy->stop();

    thread->join();
}
