#pragma once

extern "C" {
#include "saimetadata.h"
}

#include <memory>

#include "ConcurrentQueue.h"
#include "swss/selectableevent.h"

namespace syncd
{
    using PortOperStatusNotificationQueue =
            ConcurrentQueue<sai_port_oper_status_notification_t>;

    // Class to handle the port state change callback from SAI. This consists a
    // selectable event that will be used to send notification from producer thread
    // to consumer thread, and a mutex protected concurrent queue to share the port
    // state change notification data between producer and consumer threads.
    class PortStateChangeHandler
    {
        public:

            PortStateChangeHandler(
                    _In_ std::shared_ptr<swss::SelectableEvent> portStateChangeEvent);

            virtual ~PortStateChangeHandler() = default;

            // Adds the port operational status notification data to a queue and generates a
            // notification event.
            void handlePortStateChangeNotification(
                    _In_ uint32_t count,
                    _In_ const sai_port_oper_status_notification_t *data);

            // Returns the shared pointer of the queue.
            std::shared_ptr<PortOperStatusNotificationQueue> getQueue() const;

        private:

            // Choosing 4k max event queue size based on if we had 256 ports, it can
            // accommodate on average 16 port events per ports in worst case.
            static constexpr size_t PORT_STATE_CHANGE_QUEUE_SIZE = 4096;

            // SelectableEvent for producer to generate the event and for consumer to
            // listen on.
            std::shared_ptr<swss::SelectableEvent> m_portStateChangeEvent;

            // Mutex protected queue to share the data between producer and consumer.
            std::shared_ptr<PortOperStatusNotificationQueue> m_portStateChangeQueue;
    };
}  // namespace syncd
