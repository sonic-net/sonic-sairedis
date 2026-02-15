#include "NotificationPortStateChange.h"

#include "swss/logger.h"

#include "sai_serialize.h"

using namespace sairedis;

NotificationPortStateChange::NotificationPortStateChange(
        _In_ const std::string& serializedNotification):
    Notification(
            SAI_SWITCH_NOTIFICATION_TYPE_PORT_STATE_CHANGE,
            serializedNotification),
    m_portOperaStatusNotificationData(nullptr)
{
    SWSS_LOG_ENTER();

    sai_deserialize_port_oper_status_ntf(
            serializedNotification,
            m_count,
            &m_portOperaStatusNotificationData);
}

NotificationPortStateChange::~NotificationPortStateChange()
{
    SWSS_LOG_ENTER();

    sai_deserialize_free_port_oper_status_ntf(m_count, m_portOperaStatusNotificationData);
}

sai_object_id_t NotificationPortStateChange::getSwitchId() const
{
    SWSS_LOG_ENTER();

    // this notification don't contain switch id field

    return SAI_NULL_OBJECT_ID;
}

sai_object_id_t NotificationPortStateChange::getAnyObjectId() const
{
    SWSS_LOG_ENTER();

    if (m_portOperaStatusNotificationData == nullptr)
    {
        return SAI_NULL_OBJECT_ID;
    }

    for (uint32_t idx = 0; idx < m_count; idx++)
    {
        if (m_portOperaStatusNotificationData[idx].port_id != SAI_NULL_OBJECT_ID)
        {
            return m_portOperaStatusNotificationData[idx].port_id;
        }
    }

    return SAI_NULL_OBJECT_ID;
}

void NotificationPortStateChange::processMetadata(
        _In_ std::shared_ptr<saimeta::Meta> meta) const
{
    SWSS_LOG_ENTER();

    meta->meta_sai_on_port_state_change(m_count, m_portOperaStatusNotificationData);
}

void NotificationPortStateChange::executeCallback(
        _In_ const sai_switch_notifications_t& switchNotifications) const
{
    SWSS_LOG_ENTER();

    if (switchNotifications.on_port_state_change)
    {
        switchNotifications.on_port_state_change(m_count, m_portOperaStatusNotificationData);
    }
}
