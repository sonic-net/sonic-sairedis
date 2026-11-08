#include "NotificationSwitchAsicSdkHealthEvent.h"

#include "swss/logger.h"

#include "sai_serialize.h"

using namespace sairedis;

NotificationSwitchAsicSdkHealthEvent::NotificationSwitchAsicSdkHealthEvent(
        _In_ const std::string& serializedNotification):
    Notification(
            SAI_SWITCH_NOTIFICATION_TYPE_SWITCH_ASIC_SDK_HEALTH_EVENT,
            serializedNotification)
{
    SWSS_LOG_ENTER();

    sai_deserialize_switch_asic_sdk_health_event(serializedNotification,
                                                 m_switchId,
                                                 m_severity,
                                                 m_timestamp,
                                                 m_category,
                                                 m_healthData,
                                                 m_description);
}

NotificationSwitchAsicSdkHealthEvent::~NotificationSwitchAsicSdkHealthEvent()
{
    SWSS_LOG_ENTER();

    sai_deserialize_free_switch_asic_sdk_health_event(m_description);
}

sai_object_id_t NotificationSwitchAsicSdkHealthEvent::getSwitchId() const
{
    SWSS_LOG_ENTER();

    return m_switchId;
}

sai_object_id_t NotificationSwitchAsicSdkHealthEvent::getAnyObjectId() const
{
    SWSS_LOG_ENTER();

    return m_switchId;
}

void NotificationSwitchAsicSdkHealthEvent::processMetadata(
        _In_ std::shared_ptr<saimeta::Meta> meta) const
{
    SWSS_LOG_ENTER();

    meta->meta_sai_on_switch_asic_sdk_health_event(m_switchId,
                                                   m_severity,
                                                   m_timestamp,
                                                   m_category,
                                                   m_healthData,
                                                   m_description);
}

void NotificationSwitchAsicSdkHealthEvent::executeCallback(
        _In_ const sai_switch_notifications_t& switchNotifications) const
{
    SWSS_LOG_ENTER();

    if (switchNotifications.on_switch_asic_sdk_health_event)
    {
        switchNotifications.on_switch_asic_sdk_health_event(m_switchId,
                                                            m_severity,
                                                            m_timestamp,
                                                            m_category,
                                                            m_healthData,
                                                            m_description);
    }
}
