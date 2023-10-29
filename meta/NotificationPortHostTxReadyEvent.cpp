#include "NotificationPortHostTxReadyEvent.h"

#include "swss/logger.h"

#include "sai_serialize.h"

using namespace sairedis;

NotificationPortHostTxReady::NotificationPortHostTxReady(
        _In_ const std::string& serializedNotification):
    Notification(
            SAI_SWITCH_NOTIFICATION_TYPE_PORT_HOST_TX_READY,
            serializedNotification)
{
    SWSS_LOG_ENTER();

    sai_deserialize_port_host_tx_ready_ntf(
            serializedNotification,
            m_switchId,
            m_portId,
            m_portHostTxReadyStatus);
}

NotificationPortHostTxReady::~NotificationPortHostTxReady()
{
    SWSS_LOG_ENTER();
}

sai_object_id_t NotificationPortHostTxReady::getSwitchId() const
{
    SWSS_LOG_ENTER();

    return m_switchId;
}

sai_object_id_t NotificationPortHostTxReady::getAnyObjectId() const
{
    SWSS_LOG_ENTER();

    return m_portId;
}

void NotificationPortHostTxReady::processMetadata(
        _In_ std::shared_ptr<saimeta::Meta> meta) const
{
    SWSS_LOG_ENTER();

    meta->meta_sai_on_port_host_tx_ready_change(m_portId, m_switchId, m_portHostTxReadyStatus);
}

void NotificationPortHostTxReady::executeCallback(
        _In_ const sai_switch_notifications_t& switchNotifications) const
{
    SWSS_LOG_ENTER();

    if (switchNotifications.on_port_host_tx_ready)
    {
        switchNotifications.on_port_host_tx_ready(m_switchId, m_portId, m_portHostTxReadyStatus);
    }
}
