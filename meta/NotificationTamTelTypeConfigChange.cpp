#include "NotificationTamTelTypeConfigChange.h"

#include "swss/logger.h"

#include "sai_serialize.h"

using namespace sairedis;

NotificationTamTelTypeConfigChange::NotificationTamTelTypeConfigChange(
    _In_ const std::string &serializedNotification) : Notification(SAI_SWITCH_NOTIFICATION_TYPE_TAM_TEL_TYPE_CONFIG_CHANGE,
                                                                   serializedNotification)
{
    SWSS_LOG_ENTER();

    sai_deserialize_object_id(serializedNotification, m_tam_id);
}

sai_object_id_t NotificationTamTelTypeConfigChange::getSwitchId() const
{
    SWSS_LOG_ENTER();

    return SAI_NULL_OBJECT_ID;
}

sai_object_id_t NotificationTamTelTypeConfigChange::getAnyObjectId() const
{
    SWSS_LOG_ENTER();

    return SAI_NULL_OBJECT_ID;
}

void NotificationTamTelTypeConfigChange::processMetadata(
        _In_ std::shared_ptr<saimeta::Meta> meta) const
{
    SWSS_LOG_ENTER();

    meta->meta_sai_on_tam_tel_type_config_change(m_tam_id);
}

void NotificationTamTelTypeConfigChange::executeCallback(
        _In_ const sai_switch_notifications_t& switchNotifications) const
{
    SWSS_LOG_ENTER();

    if (switchNotifications.on_tam_tel_type_config_change)
    {
        switchNotifications.on_tam_tel_type_config_change(m_tam_id);
    }
}
