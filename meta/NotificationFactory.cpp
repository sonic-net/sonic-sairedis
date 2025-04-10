#include "NotificationFactory.h"
#include "NotificationFdbEvent.h"
#include "NotificationNatEvent.h"
#include "NotificationPortStateChange.h"
#include "NotificationQueuePfcDeadlock.h"
#include "NotificationSwitchShutdownRequest.h"
#include "NotificationSwitchStateChange.h"
#include "NotificationSwitchAsicSdkHealthEvent.h"
#include "NotificationBfdSessionStateChange.h"
#include "NotificationTwampSessionEvent.h"
#include "NotificationPortHostTxReadyEvent.h"
#include "NotificationTamTelTypeConfigChange.h"
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

    if (name == SAI_SWITCH_NOTIFICATION_NAME_NAT_EVENT)
        return std::make_shared<NotificationNatEvent>(serializedNotification);

    if (name == SAI_SWITCH_NOTIFICATION_NAME_PORT_HOST_TX_READY)
        return std::make_shared<NotificationPortHostTxReady>(serializedNotification);

    if (name == SAI_SWITCH_NOTIFICATION_NAME_PORT_STATE_CHANGE)
        return std::make_shared<NotificationPortStateChange>(serializedNotification);

    if (name == SAI_SWITCH_NOTIFICATION_NAME_QUEUE_PFC_DEADLOCK)
        return std::make_shared<NotificationQueuePfcDeadlock>(serializedNotification);

    if (name == SAI_SWITCH_NOTIFICATION_NAME_SWITCH_SHUTDOWN_REQUEST)
        return std::make_shared<NotificationSwitchShutdownRequest>(serializedNotification);

    if (name == SAI_SWITCH_NOTIFICATION_NAME_SWITCH_ASIC_SDK_HEALTH_EVENT)
        return std::make_shared<NotificationSwitchAsicSdkHealthEvent>(serializedNotification);

    if (name == SAI_SWITCH_NOTIFICATION_NAME_SWITCH_STATE_CHANGE)
        return std::make_shared<NotificationSwitchStateChange>(serializedNotification);

    if (name == SAI_SWITCH_NOTIFICATION_NAME_BFD_SESSION_STATE_CHANGE)
        return std::make_shared<NotificationBfdSessionStateChange>(serializedNotification);

    if (name == SAI_SWITCH_NOTIFICATION_NAME_TWAMP_SESSION_EVENT)
        return std::make_shared<NotificationTwampSessionEvent>(serializedNotification);

    if (name == SAI_SWITCH_NOTIFICATION_NAME_TAM_TEL_TYPE_CONFIG_CHANGE)
        return std::make_shared<NotificationTamTelTypeConfigChange>(serializedNotification);

    SWSS_LOG_THROW("unknown notification: '%s', FIXME", name.c_str());
}
