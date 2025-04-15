#pragma once

#include <stdint.h>

#include <swss/sal.h>
#include <vector>

namespace sairedis
{
    class OidIndexGenerator
    {
        public:

            OidIndexGenerator() = default;

            virtual ~OidIndexGenerator() = default;

        public:

            virtual uint64_t increment() = 0;

            virtual std::vector<uint64_t> incrementBy(
                _In_ uint64_t count) = 0;

            virtual void reset() = 0;
    };
}
