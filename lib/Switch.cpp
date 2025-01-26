#include "Switch.h"

#include "meta/Globals.h"

#include "swss/logger.h"

#include <cstring>

using namespace otairedis;

Switch::Switch(
        _In_ otai_object_id_t switchId):
    m_switchId(switchId)
{
    SWSS_LOG_ENTER();

    if (switchId == OTAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_THROW("switch id can't be NULL");
    }

    clearNotificationsPointers();
}

Switch::Switch(
        _In_ otai_object_id_t switchId,
        _In_ uint32_t attrCount,
        _In_ const otai_attribute_t *attrList):
    Switch(switchId)
{
    SWSS_LOG_ENTER();

    updateNotifications(attrCount, attrList);

    // OTAI_LINECARD_ATTR_LINECARD_HARDWARE_INFO is create only attribute

    m_hardwareInfo = otaimeta::Globals::getHardwareInfo(attrCount, attrList);

    SWSS_LOG_NOTICE("created switch with hwinfo = '%s'", m_hardwareInfo.c_str());
}

void Switch::clearNotificationsPointers()
{
    SWSS_LOG_ENTER();

    memset(&m_switchNotifications, 0, sizeof(m_switchNotifications));
}

otai_object_id_t Switch::getSwitchId() const
{
    SWSS_LOG_ENTER();

    return m_switchId;
}

void Switch::updateNotifications(
        _In_ uint32_t attrCount,
        _In_ const otai_attribute_t *attrList)
{
    SWSS_LOG_ENTER();

    throw;
    //otai_metadata_update_linecard_notification_pointers(&m_switchNotifications, attrCount, attrList);
}

const otai_linecard_notifications_t& Switch::getSwitchNotifications() const
{
    SWSS_LOG_ENTER();

    return m_switchNotifications;
}

const std::string& Switch::getHardwareInfo() const
{
    SWSS_LOG_ENTER();

    return m_hardwareInfo;
}
