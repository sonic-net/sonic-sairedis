#pragma once

#include "swss/sal.h"

#include <sys/types.h>

namespace saivs
{
    typedef enum _FilterPriority
    {
        MACSEC_FILTER,

    } FilterPriority;

    class TrafficFilter
    {
        public:

            typedef enum _FilterStatus
            {
                CONTINUE,

                TERMINATE,

                ERROR,

            } FilterStatus;

        public:

            TrafficFilter() = default;

            virtual ~TrafficFilter() = default;

        public:

            virtual FilterStatus execute(
                    _Inout_ void *buffer,
                    _Inout_ size_t &length) = 0;
    };
}
