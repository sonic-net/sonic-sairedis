#include "NotificationBfdSessionStateChange.h"

#include "swss/logger.h"

#include "meta/sai_serialize.h"

using namespace sairedis;

NotificationBfdSessionStateChange::NotificationBfdSessionStateChange(
        _In_ const std::string& serializedNotification):
    Notification(
            SAI_SWITCH_NOTIFICATION_TYPE_BFD_SESSION_STATE_CHANGE,
            serializedNotification),
    m_bfdSessionStateNotificationData(nullptr)
{
    SWSS_LOG_ENTER();

    sai_deserialize_bfd_session_state_ntf(
            serializedNotification,
            m_count,
            &m_bfdSessionStateNotificationData);
}

NotificationBfdSessionStateChange::~NotificationBfdSessionStateChange()
{
    SWSS_LOG_ENTER();

    sai_deserialize_free_bfd_session_state_ntf(m_count, m_bfdSessionStateNotificationData);
}

sai_object_id_t NotificationBfdSessionStateChange::getSwitchId() const
{
    SWSS_LOG_ENTER();

    // this notification don't contain switch id field

    return SAI_NULL_OBJECT_ID;
}

sai_object_id_t NotificationBfdSessionStateChange::getAnyObjectId() const
{
    SWSS_LOG_ENTER();

    if (m_bfdSessionStateNotificationData == nullptr)
    {
        return SAI_NULL_OBJECT_ID;
    }

    for (uint32_t idx = 0; idx < m_count; idx++)
    {
        if (m_bfdSessionStateNotificationData[idx].bfd_session_id != SAI_NULL_OBJECT_ID)
        {
            return m_bfdSessionStateNotificationData[idx].bfd_session_id;
        }
    }

    return SAI_NULL_OBJECT_ID;
}

void NotificationBfdSessionStateChange::processMetadata(
        _In_ std::shared_ptr<saimeta::Meta> meta) const
{
    SWSS_LOG_ENTER();

    meta->meta_sai_on_bfd_session_state_change(m_count, m_bfdSessionStateNotificationData);
}

void NotificationBfdSessionStateChange::executeCallback(
        _In_ const sai_switch_notifications_t& switchNotifications) const
{
    SWSS_LOG_ENTER();

    if (switchNotifications.on_bfd_session_state_change)
    {
        switchNotifications.on_bfd_session_state_change(m_count, m_bfdSessionStateNotificationData);
    }
}
