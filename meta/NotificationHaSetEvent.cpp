#include "NotificationHaSetEvent.h"

#include "swss/logger.h"

#include "meta/sai_serialize.h"

using namespace sairedis;

NotificationHaSetEvent::NotificationHaSetEvent(
        _In_ const std::string& serializeNotification):
    Notification(
        SAI_SWITCH_NOTIFICATION_TYPE_HA_SET_EVENT,
        serializeNotification),
    m_haSetEventData(nullptr)
{
    SWSS_LOG_ENTER();

    sai_deserialize_ha_set_event_ntf(
        serializeNotification,
        m_count,
        &m_haSetEventData);
}

NotificationHaSetEvent::~NotificationHaSetEvent()
{
    SWSS_LOG_ENTER();

    sai_deserialize_free_ha_set_event_ntf(m_count, m_haSetEventData);
}

sai_object_id_t NotificationHaSetEvent::getSwitchId() const
{
    SWSS_LOG_ENTER();

    // Ha set event does not have switch id
    return SAI_NULL_OBJECT_ID;
}

sai_object_id_t NotificationHaSetEvent::getAnyObjectId() const
{
    SWSS_LOG_ENTER();

    if (m_haSetEventData == nullptr)
    {
        return SAI_NULL_OBJECT_ID;
    }

    for (uint32_t i = 0; i < m_count; ++i)
    {
        if (m_haSetEventData[i].ha_set_id != SAI_NULL_OBJECT_ID)
        {
            return m_haSetEventData[i].ha_set_id;
        }
    }

    return SAI_NULL_OBJECT_ID;
}

void NotificationHaSetEvent::processMetadata(
        _In_ std::shared_ptr<saimeta::Meta> meta) const
{
    SWSS_LOG_ENTER();

    meta->meta_sai_on_ha_set_event(
            m_count,
            m_haSetEventData);
}

void NotificationHaSetEvent::executeCallback(
        _In_ const sai_switch_notifications_t& switchNotifications) const
{
    SWSS_LOG_ENTER();

    if(switchNotifications.on_ha_set_event)
    {
        switchNotifications.on_ha_set_event(
                m_count,
                m_haSetEventData);
    }
}
