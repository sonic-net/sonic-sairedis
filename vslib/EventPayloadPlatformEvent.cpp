#include "EventPayloadPlatformEvent.h"

#include "swss/logger.h"

using namespace saivs;

EventPayloadPlatformEvent::EventPayloadPlatformEvent(
        _In_ sai_object_id_t switchId):
    m_switchId(switchId)
{
    SWSS_LOG_ENTER();

    // empty
}

sai_object_id_t EventPayloadPlatformEvent::getSwitchId() const
{
    SWSS_LOG_ENTER();

    return m_switchId;
}
