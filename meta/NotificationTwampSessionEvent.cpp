#include "NotificationTwampSessionEvent.h"

#include "swss/logger.h"

#include "meta/sai_serialize.h"

using namespace sairedis;

NotificationTwampSessionEvent::NotificationTwampSessionEvent(
        _In_ const std::string& serializedNotification):
    Notification(
            SAI_SWITCH_NOTIFICATION_TYPE_TWAMP_SESSION_EVENT,
            serializedNotification),
    m_twampSessionEventNotificationData(nullptr)
{
    SWSS_LOG_ENTER();

    sai_deserialize_twamp_session_event_ntf(
            serializedNotification,
            m_count,
            &m_twampSessionEventNotificationData);
}

NotificationTwampSessionEvent::~NotificationTwampSessionEvent()
{
    SWSS_LOG_ENTER();

    sai_deserialize_free_twamp_session_event_ntf(m_count, m_twampSessionEventNotificationData);
}

sai_object_id_t NotificationTwampSessionEvent::getSwitchId() const
{
    SWSS_LOG_ENTER();

    // this notification don't contain switch id field

    return SAI_NULL_OBJECT_ID;
}

sai_object_id_t NotificationTwampSessionEvent::getAnyObjectId() const
{
    SWSS_LOG_ENTER();

    if (m_twampSessionEventNotificationData == nullptr)
    {
        return SAI_NULL_OBJECT_ID;
    }

    for (uint32_t idx = 0; idx < m_count; idx++)
    {
        if (m_twampSessionEventNotificationData[idx].twamp_session_id != SAI_NULL_OBJECT_ID)
        {
            return m_twampSessionEventNotificationData[idx].twamp_session_id;
        }
    }

    return SAI_NULL_OBJECT_ID;
}

void NotificationTwampSessionEvent::processMetadata(
        _In_ std::shared_ptr<saimeta::Meta> meta) const
{
    SWSS_LOG_ENTER();

    meta->meta_sai_on_twamp_session_event(m_count, m_twampSessionEventNotificationData);
}

void NotificationTwampSessionEvent::executeCallback(
        _In_ const sai_switch_notifications_t& switchNotifications) const
{
    SWSS_LOG_ENTER();

    if (switchNotifications.on_twamp_session_event)
    {
        switchNotifications.on_twamp_session_event(m_count, m_twampSessionEventNotificationData);
    }
}
