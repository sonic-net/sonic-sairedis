#include "NotificationFdbEvent.h"

#include "swss/logger.h"

#include "sai_serialize.h"

using namespace sairedis;

NotificationFdbEvent::NotificationFdbEvent(
        _In_ const std::string& serializedNotification):
    Notification(
            SAI_SWITCH_NOTIFICATION_TYPE_FDB_EVENT,
            serializedNotification),
    m_fdbEventNotificationData(nullptr)
{
    SWSS_LOG_ENTER();

    sai_deserialize_fdb_event_ntf(
            serializedNotification,
            m_count,
            &m_fdbEventNotificationData);
}

NotificationFdbEvent::~NotificationFdbEvent()
{
    SWSS_LOG_ENTER();

    sai_deserialize_free_fdb_event_ntf(m_count, m_fdbEventNotificationData);
}

sai_object_id_t NotificationFdbEvent::getSwitchId() const
{
    SWSS_LOG_ENTER();

    if (m_fdbEventNotificationData == nullptr)
    {
        return SAI_NULL_OBJECT_ID;
    }

    for (uint32_t idx = 0; idx < m_count; idx++)
    {
        auto& fdb = m_fdbEventNotificationData[idx].fdb_entry;

        if (fdb.switch_id != SAI_NULL_OBJECT_ID)
        {
            return fdb.switch_id;
        }
    }

    return SAI_NULL_OBJECT_ID;
}

sai_object_id_t NotificationFdbEvent::getAnyObjectId() const
{
    SWSS_LOG_ENTER();

    if (m_fdbEventNotificationData == nullptr)
    {
        return SAI_NULL_OBJECT_ID;
    }

    for (uint32_t idx = 0; idx < m_count; idx++)
    {
        auto& fdb = m_fdbEventNotificationData[idx].fdb_entry;

        if (fdb.switch_id != SAI_NULL_OBJECT_ID)
        {
            return fdb.switch_id;
        }

        if (fdb.bv_id != SAI_NULL_OBJECT_ID)
        {
            return fdb.bv_id;
        }

        // TODO check attribute list
        SWSS_LOG_WARN("check attribute list for any object id, FIXME");
    }

    return SAI_NULL_OBJECT_ID;
}

void NotificationFdbEvent::processMetadata(
        _In_ std::shared_ptr<saimeta::Meta> meta) const
{
    SWSS_LOG_ENTER();

    meta->meta_sai_on_fdb_event(m_count, m_fdbEventNotificationData);
}

void NotificationFdbEvent::executeCallback(
        _In_ const sai_switch_notifications_t& switchNotifications) const
{
    SWSS_LOG_ENTER();

    if (switchNotifications.on_fdb_event)
    {
        switchNotifications.on_fdb_event(m_count, m_fdbEventNotificationData);
    }
}
