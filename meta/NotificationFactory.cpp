#include "NotificationFactory.h"
#include "NotificationFdbEvent.h"
#include "NotificationPortStateChange.h"
#include "NotificationQueuePfcDeadlock.h"
#include "NotificationSwitchShutdownRequest.h"
#include "NotificationSwitchStateChange.h"
#include "sairediscommon.h"

#include "swss/logger.h"

using namespace sairedis;

std::shared_ptr<Notification> NotificationFactory::deserialize(
        _In_ const std::string& name,
        _In_ const std::string& serializedNotification)
{
    SWSS_LOG_ENTER();

    if (name == SAI_SWITCH_NOTIFICATION_NAME_FDB_EVENT)
        return std::make_shared<NotificationFdbEvent>(serializedNotification);

    if (name == SAI_SWITCH_NOTIFICATION_NAME_PORT_STATE_CHANGE)
        return std::make_shared<NotificationPortStateChange>(serializedNotification);

    if (name == SAI_SWITCH_NOTIFICATION_NAME_QUEUE_PFC_DEADLOCK)
        return std::make_shared<NotificationQueuePfcDeadlock>(serializedNotification);

    if (name == SAI_SWITCH_NOTIFICATION_NAME_SWITCH_SHUTDOWN_REQUEST)
        return std::make_shared<NotificationSwitchShutdownRequest>(serializedNotification);

    if (name == SAI_SWITCH_NOTIFICATION_NAME_SWITCH_STATE_CHANGE)
        return std::make_shared<NotificationSwitchStateChange>(serializedNotification);

    SWSS_LOG_ERROR("unknown notification: '%s', FIXME", name.c_str());

    return nullptr;
}
