#include "NotificationSwitchMacsecPostStatus.h"

#include "swss/logger.h"

#include "sai_serialize.h"

using namespace sairedis;

NotificationSwitchMacsecPostStatus::NotificationSwitchMacsecPostStatus(
        _In_ const std::string & serializedNotification):
    Notification(
            SAI_SWITCH_NOTIFICATION_TYPE_SWITCH_MACSEC_POST_STATUS,
            serializedNotification)
{
    SWSS_LOG_ENTER();

    sai_deserialize_switch_macsec_post_status_ntf(
            serializedNotification,
            m_switchId,
            m_switchMacsecPostStatusNotificationData);
}

NotificationSwitchMacsecPostStatus::~NotificationSwitchMacsecPostStatus()
{
    SWSS_LOG_ENTER();
}

sai_object_id_t NotificationSwitchMacsecPostStatus::getSwitchId() const
{
    SWSS_LOG_ENTER();

    return m_switchId;
}

sai_object_id_t NotificationSwitchMacsecPostStatus::getAnyObjectId() const
{
    SWSS_LOG_ENTER();

    // switch Id is returned in getSwitchId().

    return SAI_NULL_OBJECT_ID;
}

void NotificationSwitchMacsecPostStatus::processMetadata(
        _In_ std::shared_ptr< saimeta::Meta > meta) const
{
    SWSS_LOG_ENTER();
}

void NotificationSwitchMacsecPostStatus::executeCallback(
        _In_ const sai_switch_notifications_t & switchNotifications) const
{
    SWSS_LOG_ENTER();

    if (switchNotifications.on_switch_macsec_post_status)
    {
        switchNotifications.on_switch_macsec_post_status(m_switchId, m_switchMacsecPostStatusNotificationData);
    }
}

