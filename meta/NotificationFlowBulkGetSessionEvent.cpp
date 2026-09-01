#include "NotificationFlowBulkGetSessionEvent.h"

#include "swss/logger.h"

#include "meta/sai_serialize.h"
#include "sairediscommon.h"

using namespace sairedis;

NotificationFlowBulkGetSessionEvent::NotificationFlowBulkGetSessionEvent(
        _In_ const std::string& serializeNotification):
    Notification(
        SAI_SWITCH_NOTIFICATION_TYPE_FLOW_BULK_GET_SESSION_EVENT,
        serializeNotification),
    m_flow_bulk_session_id(SAI_NULL_OBJECT_ID),
    m_count(0),
    m_data(nullptr)
{
    SWSS_LOG_ENTER();

    sai_deserialize_flow_bulk_get_session_event_ntf(
        serializeNotification,
        m_flow_bulk_session_id,
        m_count,
        &m_data);
}

NotificationFlowBulkGetSessionEvent::~NotificationFlowBulkGetSessionEvent()
{
    SWSS_LOG_ENTER();

    sai_deserialize_free_flow_bulk_get_session_event_ntf(m_count, m_data);
}

sai_object_id_t NotificationFlowBulkGetSessionEvent::getSwitchId() const
{
    SWSS_LOG_ENTER();

    // Flow bulk get session event does not have switch id directly
    // We can extract it from flow_bulk_session_id if needed
    return SAI_NULL_OBJECT_ID;
}

sai_object_id_t NotificationFlowBulkGetSessionEvent::getAnyObjectId() const
{
    SWSS_LOG_ENTER();

    if (m_flow_bulk_session_id != SAI_NULL_OBJECT_ID)
    {
        return m_flow_bulk_session_id;
    }

    return SAI_NULL_OBJECT_ID;
}

void NotificationFlowBulkGetSessionEvent::processMetadata(
        _In_ std::shared_ptr<saimeta::Meta> meta) const
{
    SWSS_LOG_ENTER();

    meta->meta_sai_on_flow_bulk_get_session_event(
            m_flow_bulk_session_id,
            m_count,
            m_data);
}

void NotificationFlowBulkGetSessionEvent::executeCallback(
        _In_ const sai_switch_notifications_t& switchNotifications) const
{
    SWSS_LOG_ENTER();

    if (switchNotifications.on_flow_bulk_get_session_event)
    {
        switchNotifications.on_flow_bulk_get_session_event(
                m_flow_bulk_session_id,
                m_count,
                m_data);
    }
}

