#include "NotificationQueuePfcDeadlock.h"

#include "swss/logger.h"

#include "sai_serialize.h"

using namespace sairedis;

NotificationQueuePfcDeadlock::NotificationQueuePfcDeadlock(
        _In_ const std::string& serializedNotification):
    Notification(
            SAI_SWITCH_NOTIFICATION_TYPE_QUEUE_PFC_DEADLOCK,
            serializedNotification),
    m_queueDeadlockNotificationData(nullptr)
{
    SWSS_LOG_ENTER();

    sai_deserialize_queue_deadlock_ntf(
            serializedNotification,
            m_count,
            &m_queueDeadlockNotificationData);
}

NotificationQueuePfcDeadlock::~NotificationQueuePfcDeadlock()
{
    SWSS_LOG_ENTER();

    sai_deserialize_free_queue_deadlock_ntf(m_count, m_queueDeadlockNotificationData);
}

sai_object_id_t NotificationQueuePfcDeadlock::getSwitchId() const
{
    SWSS_LOG_ENTER();

    // notification don't have switch id

    return SAI_NULL_OBJECT_ID;
}

sai_object_id_t NotificationQueuePfcDeadlock::getAnyObjectId() const
{
    SWSS_LOG_ENTER();

    if (m_queueDeadlockNotificationData == nullptr)
    {
        return SAI_NULL_OBJECT_ID;
    }

    for (uint32_t idx = 0; idx < m_count; idx++)
    {
        if (m_queueDeadlockNotificationData[idx].queue_id != SAI_NULL_OBJECT_ID)
        {
            return m_queueDeadlockNotificationData[idx].queue_id;
        }
    }

    return SAI_NULL_OBJECT_ID;
}

void NotificationQueuePfcDeadlock::processMetadata(
        _In_ std::shared_ptr<saimeta::Meta> meta) const
{
    SWSS_LOG_ENTER();

    meta->meta_sai_on_queue_pfc_deadlock_notification(m_count, m_queueDeadlockNotificationData);
}

void NotificationQueuePfcDeadlock::executeCallback(
        _In_ const sai_switch_notifications_t& switchNotifications) const
{
    SWSS_LOG_ENTER();

    if (switchNotifications.on_queue_pfc_deadlock)
    {
        switchNotifications.on_queue_pfc_deadlock(m_count, m_queueDeadlockNotificationData);
    }
}
