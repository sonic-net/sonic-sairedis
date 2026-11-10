#pragma once

extern "C" {
#include "sai.h"
}

#include "EventPayload.h"

#include "swss/sal.h"

namespace saivs
{
    /**
     * @brief Event a switch platform posts from its own thread.
     *
     * The event queue thread hands it to the switch, under the API mutex
     * (SwitchStateBase::syncOnPlatformEvent), so applying it never runs
     * concurrently with an API call. A platform derives from this class to
     * carry its event data.
     */
    class EventPayloadPlatformEvent:
        public EventPayload
    {
        public:

            EventPayloadPlatformEvent(
                    _In_ sai_object_id_t switchId);

            virtual ~EventPayloadPlatformEvent() = default;

        public:

            sai_object_id_t getSwitchId() const;

        private:

            sai_object_id_t m_switchId;
    };
}
