#pragma once

#include <swss/sal.h>

#include <sys/types.h>

namespace saivs
{

    enum FilterPriority
    {
        MACSEC_FILTER,
    };

    class TrafficFilter
    {
    public:
        enum FilterStatus
        {
            CONTINUE,
            TERMINATE,
            ERROR,
        };

        virtual FilterStatus execute(
            _Inout_ void *buffer,
            _Inout_ ssize_t &length) = 0;
    };

}  // namespace saivs
