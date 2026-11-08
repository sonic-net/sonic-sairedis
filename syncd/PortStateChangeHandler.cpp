#include "PortStateChangeHandler.h"

#include <string>

#include "swss/logger.h"

using namespace syncd;

constexpr size_t PortStateChangeHandler::PORT_STATE_CHANGE_QUEUE_SIZE;

PortStateChangeHandler::PortStateChangeHandler(
        _In_ std::shared_ptr<swss::SelectableEvent> portStateChangeEvent)
{
    SWSS_LOG_ENTER();

    if (portStateChangeEvent == nullptr)
    {
        SWSS_LOG_THROW("Unexpected error: port state change event is null.");
    }

    m_portStateChangeEvent = portStateChangeEvent;

    m_portStateChangeQueue = std::make_shared<PortOperStatusNotificationQueue>(
            PORT_STATE_CHANGE_QUEUE_SIZE);
}

void PortStateChangeHandler::handlePortStateChangeNotification(
        _In_ uint32_t count,
        _In_ const sai_port_oper_status_notification_t *data)
{
    SWSS_LOG_ENTER();

    for (uint32_t idx = 0; idx < count; ++idx)
    {
        if (m_portStateChangeQueue->enqueue(data[idx]) == false)
        {
            SWSS_LOG_ERROR(
                    "Unexpected error: failed to enqueue the port state change "
                    "notification.");

            return;
        }
    }

    m_portStateChangeEvent->notify();
}

std::shared_ptr<PortOperStatusNotificationQueue> PortStateChangeHandler::getQueue() const
{
    SWSS_LOG_ENTER();

    return m_portStateChangeQueue;
}
