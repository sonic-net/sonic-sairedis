#include "NotificationNatEvent.h"

#include "swss/logger.h"

#include "sai_serialize.h"

using namespace sairedis;

NotificationNatEvent::NotificationNatEvent(
        _In_ const std::string& serializedNotification):
    Notification(
            SAI_SWITCH_NOTIFICATION_TYPE_NAT_EVENT,
            serializedNotification),
    m_natEventNotificationData(nullptr)
{
    SWSS_LOG_ENTER();

    sai_deserialize_nat_event_ntf(
            serializedNotification,
            m_count,
            &m_natEventNotificationData);
}

NotificationNatEvent::~NotificationNatEvent()
{
    SWSS_LOG_ENTER();

    sai_deserialize_free_nat_event_ntf(m_count, m_natEventNotificationData);
}

sai_object_id_t NotificationNatEvent::getSwitchId() const
{
    SWSS_LOG_ENTER();

    if (m_natEventNotificationData == nullptr)
    {
        return SAI_NULL_OBJECT_ID;
    }

    for (uint32_t idx = 0; idx < m_count; idx++)
    {
        auto& nat = m_natEventNotificationData[idx].nat_entry;

        if (nat.switch_id != SAI_NULL_OBJECT_ID)
        {
            return nat.switch_id;
        }
    }

    return SAI_NULL_OBJECT_ID;
}

sai_object_id_t NotificationNatEvent::getAnyObjectId() const
{
    SWSS_LOG_ENTER();

    if (m_natEventNotificationData == nullptr)
    {
        return SAI_NULL_OBJECT_ID;
    }

    for (uint32_t idx = 0; idx < m_count; idx++)
    {
        auto& nat = m_natEventNotificationData[idx].nat_entry;

        if (nat.switch_id != SAI_NULL_OBJECT_ID)
        {
            return nat.switch_id;
        }

        if (nat.vr_id != SAI_NULL_OBJECT_ID)
        {
            return nat.vr_id;
        }
    }

    return SAI_NULL_OBJECT_ID;
}

void NotificationNatEvent::processMetadata(
        _In_ std::shared_ptr<saimeta::Meta> meta) const
{
    SWSS_LOG_ENTER();

    meta->meta_sai_on_nat_event(m_count, m_natEventNotificationData);
}

void NotificationNatEvent::executeCallback(
        _In_ const sai_switch_notifications_t& switchNotifications) const
{
    SWSS_LOG_ENTER();

    if (switchNotifications.on_nat_event)
    {
        switchNotifications.on_nat_event(m_count, m_natEventNotificationData);
    }
}
