#include "Switch.h"

#include "meta/Globals.h"

#include "swss/logger.h"

#include <cstring>

using namespace sairedis;

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

    // SAI_SWITCH_ATTR_SWITCH_HARDWARE_INFO is create only attribute

    m_hardwareInfo = saimeta::Globals::getHardwareInfo(attrCount, attrList);

    SWSS_LOG_NOTICE("created switch with hwinfo = '%s'", m_hardwareInfo.c_str());
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

    sai_metadata_update_switch_notification_pointers(&m_switchNotifications, attrCount, attrList);
}

const sai_switch_notifications_t& Switch::getSwitchNotifications() const
{
    SWSS_LOG_ENTER();

    return m_switchNotifications;
}

const std::string& Switch::getHardwareInfo() const
{
    SWSS_LOG_ENTER();

    return m_hardwareInfo;
}
