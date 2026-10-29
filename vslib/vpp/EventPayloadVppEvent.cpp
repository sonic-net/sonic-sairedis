#include "EventPayloadVppEvent.h"

#include "swss/logger.h"

using namespace saivs;

EventPayloadVppEvent::EventPayloadVppEvent(
        _In_ sai_object_id_t switchId,
        _In_ const vpp_event_info_t& event):
    EventPayloadPlatformEvent(switchId),
    m_event(event)
{
    SWSS_LOG_ENTER();

    // The copy is not linked in the VPP event queue
    m_event.next = nullptr;
}

const vpp_event_info_t& EventPayloadVppEvent::getEvent() const
{
    SWSS_LOG_ENTER();

    return m_event;
}
