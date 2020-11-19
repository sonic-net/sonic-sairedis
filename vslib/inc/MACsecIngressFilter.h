#pragma once

#include "MACsecFilter.h"

namespace saivs
{
    class MACsecIngressFilter :
        public MACsecFilter
    {
    public:
        MACsecIngressFilter(
            _In_ const std::string &macsecInterfaceName);

        virtual ~MACsecIngressFilter() = default;

    protected:
        virtual FilterStatus forward(
            _In_ const void *buffer,
            _In_ ssize_t length) override;
    };
}
