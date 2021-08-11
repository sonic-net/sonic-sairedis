#pragma once

#include "swss/sal.h"

#include <stddef.h>
#include <sys/socket.h>

namespace saivs
{
    static constexpr size_t ETH_FRAME_BUFFER_SIZE = 0x4000;
    static constexpr size_t CONTROL_MESSAGE_BUFFER_SIZE = 0x1000;

    class TrafficForwarder
    {
        public:

            virtual ~TrafficForwarder() = default;

        protected:

            TrafficForwarder() = default;

        public:

            static void addVlanTag(
                    _Inout_ unsigned char *buffer,
                    _Inout_ size_t &length,
                    _Inout_ struct msghdr &msg);

            virtual bool sendTo(
                    _In_ int fd,
                    _In_ const unsigned char *buffer,
                    _In_ size_t length) const;
    };
}
