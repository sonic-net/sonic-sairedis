#pragma once

#include <stdint.h>

#include <saitypes.h> // For SAL annotations

namespace sairedis
{
    class OidIndexGenerator
    {
        public:

            OidIndexGenerator() = default;

            virtual ~OidIndexGenerator() = default;

        public:

            virtual uint64_t increment() = 0;
            virtual uint64_t incrementBy(
                _In_ uint64_t count) = 0;

            virtual void reset() = 0;
    };
}
