#include "DummySaiInterface.h"

#include "swss/logger.h"

#include <gtest/gtest.h>

#include <memory>

using namespace saimeta;

TEST(DummySaiInterface, queryApiVersion)
{
    DummySaiInterface sai;

    sai.apiInitialize(0,0);

    sai_api_version_t version;

    EXPECT_EQ(sai.queryApiVersion(NULL), SAI_STATUS_SUCCESS);
    EXPECT_EQ(sai.queryApiVersion(&version), SAI_STATUS_SUCCESS);
}

TEST(DummySaiInterface, bulkGet)
{
    DummySaiInterface sai;

    sai.apiInitialize(0,0);

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

TEST(DummySaiInterface, create)
{
    DummySaiInterface sai;

    sai.apiInitialize(0,0);

    sai_object_id_t oid;
    sai.create(SAI_OBJECT_TYPE_SWITCH,&oid, 0, 0, 0);

    EXPECT_NE(oid, SAI_NULL_OBJECT_ID);
}

TEST(DummySaiInterface, updateNotificationPointers)
{
    DummySaiInterface sai;

    sai.apiInitialize(0,0);

    for (uint32_t idx = 0 ; idx < sai_metadata_switch_notify_attr_count; idx++)
    {
        sai_attribute_t attr;

        attr.id = sai_metadata_switch_notify_attr[idx]->attrid;
        attr.value.ptr = NULL;

        auto status = sai.set(SAI_OBJECT_TYPE_SWITCH, 0x0, &attr);

        EXPECT_EQ(status, SAI_STATUS_SUCCESS);
    }
}

TEST(DummySaiInterface, start_stop)
{
    DummySaiInterface sai;

    EXPECT_EQ(sai.start(), SAI_STATUS_FAILURE); // api not initialized

    EXPECT_EQ(sai.apiInitialize(0,0), SAI_STATUS_SUCCESS);

    EXPECT_EQ(sai.stop(), SAI_STATUS_SUCCESS); // not running

    EXPECT_EQ(sai.start(), SAI_STATUS_SUCCESS); // api initialized

    EXPECT_EQ(sai.start(), SAI_STATUS_SUCCESS); // 2nd time is ok to start, already running

    sleep(1);

    EXPECT_EQ(sai.stop(), SAI_STATUS_SUCCESS);
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

void onTwampSessionEvent(
        _In_ uint32_t count,
        _In_ const sai_twamp_session_event_notification_data_t *data)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("received: onTwampSessionEvent");

    ntfCounter++;
}

void onTamTelTypeConfigChange(
        _In_ sai_object_id_t tam_tel_id)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("received: onTamTelTypeConfigChange");

    ntfCounter++;
}

TEST(DummySaiInterface, sendNotification)
{
    DummySaiInterface sai;

    ntfCounter = 0;

    // api not initialized
    EXPECT_EQ(sai.enqueueNotificationToSend(SAI_SWITCH_ATTR_SWITCH_STATE_CHANGE_NOTIFY), SAI_STATUS_FAILURE);

    EXPECT_EQ(sai.apiInitialize(0,0), SAI_STATUS_SUCCESS);

    EXPECT_EQ(sai.enqueueNotificationToSend(SAI_SWITCH_ATTR_SWITCH_ASIC_SDK_HEALTH_EVENT_NOTIFY), SAI_STATUS_SUCCESS);
    EXPECT_EQ(sai.enqueueNotificationToSend(SAI_SWITCH_ATTR_NAT_EVENT_NOTIFY), SAI_STATUS_SUCCESS);
    EXPECT_EQ(sai.enqueueNotificationToSend(SAI_SWITCH_ATTR_PORT_HOST_TX_READY_NOTIFY), SAI_STATUS_SUCCESS);
    EXPECT_EQ(sai.enqueueNotificationToSend(SAI_SWITCH_ATTR_QUEUE_PFC_DEADLOCK_NOTIFY), SAI_STATUS_SUCCESS);
    EXPECT_EQ(sai.enqueueNotificationToSend(SAI_SWITCH_ATTR_BFD_SESSION_STATE_CHANGE_NOTIFY), SAI_STATUS_SUCCESS);
    EXPECT_EQ(sai.enqueueNotificationToSend(SAI_SWITCH_ATTR_TWAMP_SESSION_EVENT_NOTIFY), SAI_STATUS_SUCCESS);

    EXPECT_EQ(sai.enqueueNotificationToSend(SAI_SWITCH_ATTR_SWITCH_STATE_CHANGE_NOTIFY), SAI_STATUS_SUCCESS);
    EXPECT_EQ(sai.enqueueNotificationToSend(SAI_SWITCH_ATTR_FDB_EVENT_NOTIFY), SAI_STATUS_SUCCESS);
    EXPECT_EQ(sai.enqueueNotificationToSend(SAI_SWITCH_ATTR_PORT_STATE_CHANGE_NOTIFY), SAI_STATUS_SUCCESS);
    EXPECT_EQ(sai.enqueueNotificationToSend(SAI_SWITCH_ATTR_SHUTDOWN_REQUEST_NOTIFY), SAI_STATUS_SUCCESS);

    EXPECT_EQ(sai.enqueueNotificationToSend(SAI_SWITCH_ATTR_TAM_TEL_TYPE_CONFIG_CHANGE_NOTIFY), SAI_STATUS_SUCCESS);

    sai_attribute_t attr;

    sai_object_id_t switch_id = 0x1;

    attr.id = SAI_SWITCH_ATTR_SWITCH_STATE_CHANGE_NOTIFY;
    attr.value.ptr = (void*)&onSwitchStateChange;
    sai.set(SAI_OBJECT_TYPE_SWITCH, switch_id, &attr);

    attr.id = SAI_SWITCH_ATTR_FDB_EVENT_NOTIFY;
    attr.value.ptr = (void*)&onFdbEvent;
    sai.set(SAI_OBJECT_TYPE_SWITCH, switch_id, &attr);

    attr.id = SAI_SWITCH_ATTR_PORT_STATE_CHANGE_NOTIFY;
    attr.value.ptr = (void*)&onPortStateChange;
    sai.set(SAI_OBJECT_TYPE_SWITCH, switch_id, &attr);

    attr.id = SAI_SWITCH_ATTR_SHUTDOWN_REQUEST_NOTIFY;
    attr.value.ptr = (void*)&onSwitchShutdownRequest;
    sai.set(SAI_OBJECT_TYPE_SWITCH, switch_id, &attr);


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

    EXPECT_EQ(sai.start(), SAI_STATUS_SUCCESS);

    sleep(1);

    EXPECT_EQ(sai.stop(), SAI_STATUS_SUCCESS);

    EXPECT_EQ(ntfCounter, 4 + 6 + 1);
}
