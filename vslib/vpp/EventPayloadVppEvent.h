#pragma once

#include "EventPayloadPlatformEvent.h"

#include "vppxlate/SaiVppXlate.h"

namespace saivs
{
    /**
     * @brief VPP link or BFD state event, which the VPP events thread posts for
     * SwitchVpp::syncOnPlatformEvent to apply under the API mutex.
     */
    class EventPayloadVppEvent:
        public EventPayloadPlatformEvent
    {
        public:

            EventPayloadVppEvent(
                    _In_ sai_object_id_t switchId,
                    _In_ const vpp_event_info_t& event);

            virtual ~EventPayloadVppEvent() = default;

        public:

            const vpp_event_info_t& getEvent() const;

        private:

            vpp_event_info_t m_event;
    };
}
