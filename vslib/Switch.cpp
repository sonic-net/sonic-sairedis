#include "Switch.h"

#include "swss/logger.h"

#include <cstring>

using namespace saivs;

Switch::Switch(
        _In_ sai_object_id_t switchId):
    m_switchId(switchId)
{
    SWSS_LOG_ENTER();

    if (switchId == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_THROW("switch id can't be NULL");
    }

    clearNotificationsPointers();
}

Switch::Switch(
        _In_ sai_object_id_t switchId,
        _In_ uint32_t attrCount,
        _In_ const sai_attribute_t *attrList):
    Switch(switchId)
{
    SWSS_LOG_ENTER();

    updateNotifications(attrCount, attrList);
}

void Switch::clearNotificationsPointers()
{
    SWSS_LOG_ENTER();

    memset(&m_switchNotifications, 0, sizeof(m_switchNotifications));
}

sai_object_id_t Switch::getSwitchId() const
{
    SWSS_LOG_ENTER();

    return m_switchId;
}

void Switch::updateNotifications(
        _In_ uint32_t attrCount,
        _In_ const sai_attribute_t *attrList)
{
    SWSS_LOG_ENTER();

    /*
     * This function should only be called on CREATE/SET
     * api when object is SWITCH.
     */

    if (attrCount && attrList == nullptr)
    {
        SWSS_LOG_THROW("attrCount is %u, but attrList is nullptr", attrCount);
    }

    sai_metadata_update_switch_notification_pointers(&m_switchNotifications, attrCount, attrList);
}

const sai_switch_notifications_t& Switch::getSwitchNotifications() const
{
    SWSS_LOG_ENTER();

    return m_switchNotifications;
}
