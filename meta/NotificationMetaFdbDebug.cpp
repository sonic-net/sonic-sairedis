#include "NotificationMetaFdbDebug.h"

#include "swss/logger.h"

using namespace sairedis;

NotificationMetaFdbDebug::NotificationMetaFdbDebug(
        _In_ const std::string& op,
        _In_ const std::string& data):
    Notification(
            // Not a real switch notification; reuse an existing enum value for
            // the base class. The type is never dispatched on for this object.
            SAI_SWITCH_NOTIFICATION_TYPE_FDB_EVENT,
            op + "|" + data),
    m_op(op),
    m_data(data)
{
    SWSS_LOG_ENTER();

    // no deserialization: op/data are carried verbatim
}

sai_object_id_t NotificationMetaFdbDebug::getSwitchId() const
{
    SWSS_LOG_ENTER();

    return SAI_NULL_OBJECT_ID;
}

sai_object_id_t NotificationMetaFdbDebug::getAnyObjectId() const
{
    SWSS_LOG_ENTER();

    return SAI_NULL_OBJECT_ID;
}

void NotificationMetaFdbDebug::processMetadata(
        _In_ std::shared_ptr<saimeta::Meta> meta) const
{
    SWSS_LOG_ENTER();

    // Executed under the sairedis API mutex (see Notification::processMetadata
    // contract). processDebugCommand is tolerant and never throws.

    meta->processDebugCommand(m_op, m_data);
}

void NotificationMetaFdbDebug::executeCallback(
        _In_ const sai_switch_notifications_t& switchNotifications) const
{
    SWSS_LOG_ENTER();

    // Intentionally empty: operator debug commands have no SAI-level callback.
    (void)switchNotifications;
}
