#pragma once

extern "C" {
#include "sai.h"
}

#include "EventPayload.h"

#include "swss/sal.h"

#include <string>

namespace saivs
{
    class EventPayloadNetLinkMsg:
        public EventPayload
    {
        public:

            EventPayloadNetLinkMsg(
                    _In_ sai_object_id_t switchId,
                    _In_ int nlmsgType,
                    _In_ int ifIndex,
                    _In_ unsigned int ifFlags,
                    _In_ const std::string& ifName);

            virtual ~EventPayloadNetLinkMsg() = default;

        public:

            sai_object_id_t getSwitchId() const;

            int getNlmsgType() const;

            int getIfIndex() const;

            unsigned int getIfFlags() const;

            const std::string& getIfName() const;

        private:

            sai_object_id_t m_switchId;

            int m_nlmsgType;

            int m_ifIndex;

            unsigned int m_ifFlags;

            std::string m_ifName;
    };
}
