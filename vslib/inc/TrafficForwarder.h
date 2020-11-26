#pragma once

#include "swss/sal.h"

#include <stddef.h>
#include <sys/socket.h>

namespace saivs
{
    class TrafficForwarder
    {
    public:
        virtual ~TrafficForwarder() = default;

    protected:
        TrafficForwarder() = default;

        void add_vlan_tag(
            _Inout_ unsigned char *buffer,
            _Inout_ size_t &length,
            _Inout_ struct msghdr &msg) const;

        bool send_to(
            _In_ int fd,
            _In_ const unsigned char *buffer,
            _In_ size_t &length) const;

    };
}
