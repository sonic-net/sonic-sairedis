#pragma once

#include <stdint.h>

namespace otairedis
{
    class OidIndexGenerator
    {
        public:

            OidIndexGenerator() = default;

            virtual ~OidIndexGenerator() = default;

        public:

            virtual uint64_t increment() = 0;

            virtual void reset() = 0;
    };
}
