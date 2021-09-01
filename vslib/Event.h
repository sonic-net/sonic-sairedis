#pragma once

#include "EventPayload.h"

#include "swss/sal.h"

#include <memory>

namespace saivs
{
    typedef enum _EventType
    {
        EVENT_TYPE_END_THREAD,

        EVENT_TYPE_NET_LINK_MSG,

        EVENT_TYPE_PACKET,

        EVENT_TYPE_NOTIFICATION,

    } EventType;

    class Event
    {
        public:

            Event(
                    _In_ EventType eventType,
                    _In_ std::shared_ptr<EventPayload> payload);

            virtual ~Event() = default;

        public:

            EventType getType() const;

            std::shared_ptr<EventPayload> getPayload() const;

        private:

            EventType m_eventType;

            std::shared_ptr<EventPayload> m_payload;
    };
}
