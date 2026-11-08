#include "NotificationIcmpEchoSessionStateChange.h"

#include "swss/logger.h"

#include "meta/sai_serialize.h"

using namespace sairedis;

NotificationIcmpEchoSessionStateChange::NotificationIcmpEchoSessionStateChange(
        _In_ const std::string& serializedNotification):
    Notification(
            SAI_SWITCH_NOTIFICATION_TYPE_ICMP_ECHO_SESSION_STATE_CHANGE,
            serializedNotification),
    m_icmpEchoSessionStateNotificationData(nullptr)
{
    SWSS_LOG_ENTER();

    sai_deserialize_icmp_echo_session_state_ntf(
            serializedNotification,
            m_count,
            &m_icmpEchoSessionStateNotificationData);
}

NotificationIcmpEchoSessionStateChange::~NotificationIcmpEchoSessionStateChange()
{
    SWSS_LOG_ENTER();

    sai_deserialize_free_icmp_echo_session_state_ntf(m_count, m_icmpEchoSessionStateNotificationData);
}

sai_object_id_t NotificationIcmpEchoSessionStateChange::getSwitchId() const
{
    SWSS_LOG_ENTER();

    // this notification don't contain switch id field

    return SAI_NULL_OBJECT_ID;
}

sai_object_id_t NotificationIcmpEchoSessionStateChange::getAnyObjectId() const
{
    SWSS_LOG_ENTER();

    if (m_icmpEchoSessionStateNotificationData == nullptr)
    {
        return SAI_NULL_OBJECT_ID;
    }

    for (uint32_t idx = 0; idx < m_count; idx++)
    {
        if (m_icmpEchoSessionStateNotificationData[idx].icmp_echo_session_id != SAI_NULL_OBJECT_ID)
        {
            return m_icmpEchoSessionStateNotificationData[idx].icmp_echo_session_id;
        }
    }

    return SAI_NULL_OBJECT_ID;
}

void NotificationIcmpEchoSessionStateChange::processMetadata(
        _In_ std::shared_ptr<saimeta::Meta> meta) const
{
    SWSS_LOG_ENTER();

    meta->meta_sai_on_icmp_echo_session_state_change(m_count, m_icmpEchoSessionStateNotificationData);
}

void NotificationIcmpEchoSessionStateChange::executeCallback(
        _In_ const sai_switch_notifications_t& switchNotifications) const
{
    SWSS_LOG_ENTER();

    if (switchNotifications.on_icmp_echo_session_state_change)
    {
        switchNotifications.on_icmp_echo_session_state_change(m_count, m_icmpEchoSessionStateNotificationData);
    }
}
