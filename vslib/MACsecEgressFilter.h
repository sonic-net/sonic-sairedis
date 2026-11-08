#pragma once

#include "MACsecFilter.h"

namespace saivs
{
    class MACsecEgressFilter:
        public MACsecFilter
    {
        public:

            MACsecEgressFilter(
                    _In_ const std::string &macsecInterfaceName);

            virtual ~MACsecEgressFilter() = default;

        protected:

            virtual FilterStatus forward(
                    _In_ const void *buffer,
                    _In_ size_t length) override;
    };
}
