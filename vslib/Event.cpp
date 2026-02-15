#include "Event.h"

#include "swss/logger.h"

using namespace saivs;

Event::Event(
        _In_ EventType eventType,
        _In_ std::shared_ptr<EventPayload> payload):
    m_eventType(eventType),
    m_payload(payload)
{
    SWSS_LOG_ENTER();

    // empty
}

EventType Event::getType() const
{
    SWSS_LOG_ENTER();

    return m_eventType;
}

std::shared_ptr<EventPayload> Event::getPayload() const
{
    SWSS_LOG_ENTER();

    return m_payload;
}
