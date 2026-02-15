#include "NotificationSwitchShutdownRequest.h"

#include "swss/logger.h"

#include "sai_serialize.h"

using namespace sairedis;

NotificationSwitchShutdownRequest::NotificationSwitchShutdownRequest(
        _In_ const std::string& serializedNotification):
    Notification(
            SAI_SWITCH_NOTIFICATION_TYPE_SWITCH_SHUTDOWN_REQUEST,
            serializedNotification)
{
    SWSS_LOG_ENTER();

    sai_deserialize_switch_shutdown_request(serializedNotification, m_switchId);
}

sai_object_id_t NotificationSwitchShutdownRequest::getSwitchId() const
{
    SWSS_LOG_ENTER();

    return m_switchId;
}

sai_object_id_t NotificationSwitchShutdownRequest::getAnyObjectId() const
{
    SWSS_LOG_ENTER();

    return m_switchId;
}

void NotificationSwitchShutdownRequest::processMetadata(
        _In_ std::shared_ptr<saimeta::Meta> meta) const
{
    SWSS_LOG_ENTER();

    meta->meta_sai_on_switch_shutdown_request(m_switchId);
}

void NotificationSwitchShutdownRequest::executeCallback(
        _In_ const sai_switch_notifications_t& switchNotifications) const
{
    SWSS_LOG_ENTER();

    if (switchNotifications.on_switch_shutdown_request)
    {
        switchNotifications.on_switch_shutdown_request(m_switchId);
    }
}
