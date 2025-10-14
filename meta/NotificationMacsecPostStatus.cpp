#include "NotificationMacsecPostStatus.h"

#include "swss/logger.h"

#include "sai_serialize.h"

using namespace sairedis;

NotificationMacsecPostStatus::NotificationMacsecPostStatus(
        _In_ const std::string & serializedNotification):
    Notification(
            SAI_SWITCH_NOTIFICATION_TYPE_MACSEC_POST_STATUS,
            serializedNotification)
{
    SWSS_LOG_ENTER();

    sai_deserialize_macsec_post_status_ntf(
            serializedNotification,
            m_macsecId,
            m_macsecPostStatusNotificationData);
}

NotificationMacsecPostStatus::~NotificationMacsecPostStatus()
{
    SWSS_LOG_ENTER();
}

sai_object_id_t NotificationMacsecPostStatus::getSwitchId() const
{
    SWSS_LOG_ENTER();

    // this notification don't contain switch id field

    return SAI_NULL_OBJECT_ID;
}

sai_object_id_t NotificationMacsecPostStatus::getAnyObjectId() const
{
    SWSS_LOG_ENTER();

    return m_macsecId;
}

void NotificationMacsecPostStatus::processMetadata(
        _In_ std::shared_ptr< saimeta::Meta > meta) const
{
    SWSS_LOG_ENTER();
}

void NotificationMacsecPostStatus::executeCallback(
        _In_ const sai_switch_notifications_t & switchNotifications) const
{
    SWSS_LOG_ENTER();

    if (switchNotifications.on_macsec_post_status)
    {
        switchNotifications.on_macsec_post_status(m_macsecId, m_macsecPostStatusNotificationData);
    }
}


