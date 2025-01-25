#include "Notification.h"

#include "swss/logger.h"

using namespace otairedis;

Notification::Notification(
        _In_ otai_linecard_notification_type_t switchNotificationType,
        _In_ const std::string& serializedNotification):
    m_switchNotificationType(switchNotificationType),
    m_serializedNotification(serializedNotification)
{
    SWSS_LOG_ENTER();

    // empty
}

otai_linecard_notification_type_t Notification::getNotificationType() const
{
    SWSS_LOG_ENTER();

    return m_switchNotificationType;
}

const std::string& Notification::getSerializedNotification() const
{
    SWSS_LOG_ENTER();

    return m_serializedNotification;
}
