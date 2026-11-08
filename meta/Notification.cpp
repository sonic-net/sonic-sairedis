#include "Notification.h"

#include "swss/logger.h"

using namespace sairedis;

Notification::Notification(
        _In_ sai_switch_notification_type_t switchNotificationType,
        _In_ const std::string& serializedNotification):
    m_switchNotificationType(switchNotificationType),
    m_serializedNotification(serializedNotification)
{
    SWSS_LOG_ENTER();

    // empty
}

sai_switch_notification_type_t Notification::getNotificationType() const
{
    SWSS_LOG_ENTER();

    return m_switchNotificationType;
}

const std::string& Notification::getSerializedNotification() const
{
    SWSS_LOG_ENTER();

    return m_serializedNotification;
}
