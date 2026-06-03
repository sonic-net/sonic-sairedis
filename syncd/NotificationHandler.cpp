#include "NotificationHandler.h"
#include "FlowDump.h"
#include "Workaround.h"
#include "sairediscommon.h"

#include "swss/logger.h"

#include "meta/sai_serialize.h"

#include <inttypes.h>

using namespace syncd;

NotificationHandler::NotificationHandler(
        _In_ std::shared_ptr<NotificationProcessor> processor,
        _In_ sai_api_version_t apiVersion):
    m_processor(processor),
    m_apiVersion(apiVersion)
{
    SWSS_LOG_ENTER();

    memset(&m_switchNotifications, 0, sizeof(m_switchNotifications));

    m_notificationQueue = processor->getQueue();
}

NotificationHandler::~NotificationHandler()
{
    SWSS_LOG_ENTER();

    // empty
}

void NotificationHandler::setApiVersion(
        _In_ sai_api_version_t apiVersion)
{
    SWSS_LOG_ENTER();

    m_apiVersion = apiVersion;
}

sai_api_version_t NotificationHandler::getApiVersion() const
{
    SWSS_LOG_ENTER();

    return m_apiVersion;
}

void NotificationHandler::setSwitchNotifications(
        _In_ const sai_switch_notifications_t& switchNotifications)
{
    SWSS_LOG_ENTER();

    m_switchNotifications = switchNotifications;
}

const sai_switch_notifications_t& NotificationHandler::getSwitchNotifications() const
{
    SWSS_LOG_ENTER();

    return m_switchNotifications;
}

void NotificationHandler::updateNotificationsPointers(
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list) const
{
    SWSS_LOG_ENTER();

    /*
     * This function should only be called on CREATE/SET api when object is
     * SWITCH.
     *
     * Notifications pointers needs to be corrected since those we receive from
     * sairedis are in sairedis memory space and here we are using those ones
     * we declared in syncd memory space.
     *
     * Also notice that we are using the same pointers for ALL switches.
     */

    sai_metadata_update_attribute_notification_pointers(&m_switchNotifications, attr_count, attr_list);
}

// TODO use same Notification class from sairedis lib
// then this will handle deserialize free

void NotificationHandler::onFdbEvent(
        _In_ uint32_t count,
        _In_ const sai_fdb_event_notification_data_t *data)
{
    SWSS_LOG_ENTER();

    std::string s = sai_serialize_fdb_event_ntf(count, data);

    enqueueNotification(SAI_SWITCH_NOTIFICATION_NAME_FDB_EVENT, s);
}

void NotificationHandler::onNatEvent(
        _In_ uint32_t count,
        _In_ const sai_nat_event_notification_data_t *data)
{
    SWSS_LOG_ENTER();

    std::string s = sai_serialize_nat_event_ntf(count, data);

    enqueueNotification(SAI_SWITCH_NOTIFICATION_NAME_NAT_EVENT, s);
}

void NotificationHandler::onPortStateChange(
        _In_ uint32_t count,
        _In_ const sai_port_oper_status_notification_t *data)
{
    SWSS_LOG_ENTER();

    auto ntfdata = Workaround::convertPortOperStatusNotification(count, data, m_apiVersion);

    auto s = sai_serialize_port_oper_status_ntf((uint32_t)ntfdata.size(), ntfdata.data());

    enqueueNotification(SAI_SWITCH_NOTIFICATION_NAME_PORT_STATE_CHANGE, s);
}

void NotificationHandler::onPortHostTxReady(
        _In_ sai_object_id_t switch_id,
        _In_ sai_object_id_t port_id,
        _In_ sai_port_host_tx_ready_status_t host_tx_ready_status)
{
    SWSS_LOG_ENTER();

    auto s = sai_serialize_port_host_tx_ready_ntf(switch_id, port_id, host_tx_ready_status);

    enqueueNotification(SAI_SWITCH_NOTIFICATION_NAME_PORT_HOST_TX_READY, s);
}

void NotificationHandler::onQueuePfcDeadlock(
        _In_ uint32_t count,
        _In_ const sai_queue_deadlock_notification_data_t *data)
{
    SWSS_LOG_ENTER();

    auto s = sai_serialize_queue_deadlock_ntf(count, data);

    enqueueNotification(SAI_SWITCH_NOTIFICATION_NAME_QUEUE_PFC_DEADLOCK, s);
}

void NotificationHandler::onSwitchShutdownRequest(
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();

    auto s = sai_serialize_switch_shutdown_request(switch_id);

    enqueueNotification(SAI_SWITCH_NOTIFICATION_NAME_SWITCH_SHUTDOWN_REQUEST, s);
}

void  NotificationHandler::onSwitchAsicSdkHealthEvent(
        _In_ sai_object_id_t switch_id,
        _In_ sai_switch_asic_sdk_health_severity_t severity,
        _In_ sai_timespec_t timestamp,
        _In_ sai_switch_asic_sdk_health_category_t category,
        _In_ sai_switch_health_data_t data,
        _In_ const sai_u8_list_t description)
{
    SWSS_LOG_ENTER();

    std::string s = sai_serialize_switch_asic_sdk_health_event(switch_id, severity, timestamp, category, data, description);

    enqueueNotification(SAI_SWITCH_NOTIFICATION_NAME_SWITCH_ASIC_SDK_HEALTH_EVENT, s);
}

void NotificationHandler::onSwitchStateChange(
        _In_ sai_object_id_t switch_id,
        _In_ sai_switch_oper_status_t switch_oper_status)
{
    SWSS_LOG_ENTER();

    auto s = sai_serialize_switch_oper_status(switch_id, switch_oper_status);

    enqueueNotification(SAI_SWITCH_NOTIFICATION_NAME_SWITCH_STATE_CHANGE, s);
}

void NotificationHandler::onBfdSessionStateChange(
        _In_ uint32_t count,
        _In_ const sai_bfd_session_state_notification_t *data)
{
    SWSS_LOG_ENTER();

    std::string s = sai_serialize_bfd_session_state_ntf(count, data);

    enqueueNotification(SAI_SWITCH_NOTIFICATION_NAME_BFD_SESSION_STATE_CHANGE, s);
}

void NotificationHandler::onIcmpEchoSessionStateChange(
        _In_ uint32_t count,
        _In_ const sai_icmp_echo_session_state_notification_t *data)
{
    SWSS_LOG_ENTER();

    std::string s = sai_serialize_icmp_echo_session_state_ntf(count, data);

    enqueueNotification(SAI_SWITCH_NOTIFICATION_NAME_ICMP_ECHO_SESSION_STATE_CHANGE, s);
}

void NotificationHandler::onHaSetEvent(
        _In_ uint32_t count,
        _In_ const sai_ha_set_event_data_t *data)
{
    SWSS_LOG_ENTER();

    std::string s = sai_serialize_ha_set_event_ntf(count, data);

    enqueueNotification(SAI_SWITCH_NOTIFICATION_NAME_HA_SET_EVENT, s);
}

void NotificationHandler::onHaScopeEvent(
        _In_ uint32_t count,
        _In_ const sai_ha_scope_event_data_t *data)
{
    SWSS_LOG_ENTER();

    std::string s = sai_serialize_ha_scope_event_ntf(count, data);

    enqueueNotification(SAI_SWITCH_NOTIFICATION_NAME_HA_SCOPE_EVENT, s);
}

void NotificationHandler::onFlowBulkGetSessionEvent(
        _In_ sai_object_id_t flow_bulk_session_id,
        _In_ uint32_t count,
        _In_ const sai_flow_bulk_get_session_event_data_t *data)
{
    SWSS_LOG_ENTER();

    FlowDumpDataPtr flow_dump_data = std::make_shared<FlowDumpData>();
    std::string s = sai_serialize_flow_bulk_get_session_event_ntf(flow_bulk_session_id, count, data);

    for (uint32_t i = 0; i < count; ++i)
    {
        if (data[i].event_type == SAI_FLOW_BULK_GET_SESSION_EVENT_FLOW_ENTRY)
        {
            try
            {
                nlohmann::json json_line = FlowDumpSerializer::serializeFlowEntryToJson(data[i]);
                flow_dump_data->json_lines.push_back(json_line);
            }
            catch (const std::exception& e)
            {
                SWSS_LOG_WARN("Failed to serialize flow entry at index %u: %s", i, e.what());
                continue;
            }
        }
    }

    enqueueNotification(SAI_SWITCH_NOTIFICATION_NAME_FLOW_BULK_GET_SESSION_EVENT, s, flow_dump_data);
}

void NotificationHandler::enqueueNotification(
        _In_ const std::string& op,
        _In_ const std::string& data,
        _In_ const std::vector<swss::FieldValueTuple> &entry)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("%s %s", op.c_str(), data.c_str());

    swss::KeyOpFieldsValuesTuple item(op, data, entry);

    if (m_notificationQueue->enqueue(item))
    {
        m_processor->signal();
    }
}

void NotificationHandler::enqueueNotification(
        _In_ const std::string& op,
        _In_ const std::string& data,
        _In_ FlowDumpDataPtr auxiliary_data)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("%s %s", op.c_str(), data.c_str());

    swss::KeyOpFieldsValuesTuple item(op, data, std::vector<swss::FieldValueTuple>());

    if (m_notificationQueue->enqueue(item, auxiliary_data))
    {
        m_processor->signal();
    }
}

void NotificationHandler::onTwampSessionEvent(
        _In_ uint32_t count,
        _In_ const sai_twamp_session_event_notification_data_t *data)
{
    SWSS_LOG_ENTER();

    std::string s = sai_serialize_twamp_session_event_ntf(count, data);

    enqueueNotification(SAI_SWITCH_NOTIFICATION_NAME_TWAMP_SESSION_EVENT, s);
}

void NotificationHandler::onTamTelTypeConfigChange(
    _In_ sai_object_id_t tam_tel_id)
{
    SWSS_LOG_ENTER();

    std::string s = sai_serialize_object_id(tam_tel_id);

    enqueueNotification(SAI_SWITCH_NOTIFICATION_NAME_TAM_TEL_TYPE_CONFIG_CHANGE, s);
}

void NotificationHandler::onSwitchMacsecPostStatus(
        _In_ sai_object_id_t switch_id,
        _In_ sai_switch_macsec_post_status_t switch_macsec_post_status)
{
    SWSS_LOG_ENTER();

    std::string s = sai_serialize_switch_macsec_post_status_ntf(switch_id, switch_macsec_post_status);

    enqueueNotification(SAI_SWITCH_NOTIFICATION_NAME_SWITCH_MACSEC_POST_STATUS, s);
}

void NotificationHandler::onMacsecPostStatus(
        _In_ sai_object_id_t macsec_id,
        _In_ sai_macsec_post_status_t macsec_post_status)
{
    SWSS_LOG_ENTER();

    std::string s = sai_serialize_macsec_post_status_ntf(macsec_id, macsec_post_status);

    enqueueNotification(SAI_SWITCH_NOTIFICATION_NAME_MACSEC_POST_STATUS, s);
}

void NotificationHandler::enqueueNotification(
        _In_ const std::string& op,
        _In_ const std::string& data)
{
    SWSS_LOG_ENTER();

    std::vector<swss::FieldValueTuple> entry;

    enqueueNotification(op, data, entry);
}

