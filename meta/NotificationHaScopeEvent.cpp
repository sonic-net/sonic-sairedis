#include "NotificationHaScopeEvent.h"

#include "swss/logger.h"

#include "meta/sai_serialize.h"

using namespace sairedis;

NotificationHaScopeEvent::NotificationHaScopeEvent(
        _In_ const std::string& serializeNotification):
    Notification(
        SAI_SWITCH_NOTIFICATION_TYPE_HA_SCOPE_EVENT,
        serializeNotification),
    m_haScopeEventData(nullptr)
{
    SWSS_LOG_ENTER();

    sai_deserialize_ha_scope_event_ntf(
        serializeNotification,
        m_count,
        &m_haScopeEventData);
}

NotificationHaScopeEvent::~NotificationHaScopeEvent()
{
    SWSS_LOG_ENTER();

    sai_deserialize_free_ha_scope_event_ntf(m_count, m_haScopeEventData);
}

sai_object_id_t NotificationHaScopeEvent::getSwitchId() const
{
    SWSS_LOG_ENTER();

    // Ha scope event does not have switch id
    return SAI_NULL_OBJECT_ID;
}

sai_object_id_t NotificationHaScopeEvent::getAnyObjectId() const
{
    SWSS_LOG_ENTER();

    if (m_haScopeEventData == nullptr)
    {
        return SAI_NULL_OBJECT_ID;
    }

    for (uint32_t i = 0; i < m_count; ++i)
    {
        if (m_haScopeEventData[i].ha_scope_id != SAI_NULL_OBJECT_ID)
        {
            return m_haScopeEventData[i].ha_scope_id;
        }
    }

    return SAI_NULL_OBJECT_ID;
}

void NotificationHaScopeEvent::processMetadata(
        _In_ std::shared_ptr<saimeta::Meta> meta) const
{
    SWSS_LOG_ENTER();

    meta->meta_sai_on_ha_scope_event(
            m_count,
            m_haScopeEventData);
}

void NotificationHaScopeEvent::executeCallback(
        _In_ const sai_switch_notifications_t& switchNotifications) const
{
    SWSS_LOG_ENTER();

    if (switchNotifications.on_ha_scope_event)
    {
        switchNotifications.on_ha_scope_event(m_count, m_haScopeEventData);
    }
}
