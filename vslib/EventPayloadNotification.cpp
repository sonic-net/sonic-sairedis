#include "EventPayloadNotification.h"

#include "swss/logger.h"

using namespace saivs;

EventPayloadNotification::EventPayloadNotification(
        _In_ std::shared_ptr<sairedis::Notification> ntf,
        _In_ const sai_switch_notifications_t& switchNotifications):
    m_ntf(ntf),
    m_switchNotifications(switchNotifications)
{
    SWSS_LOG_ENTER();

    // empty
}

std::shared_ptr<sairedis::Notification> EventPayloadNotification::getNotification() const
{
    SWSS_LOG_ENTER();

    return m_ntf;
}

const sai_switch_notifications_t& EventPayloadNotification::getSwitchNotifications() const
{
    SWSS_LOG_ENTER();

    return m_switchNotifications;
}
