#pragma once

#include "TrafficFilter.h"

#include <string>

namespace saivs
{

    class MACsecFilter
        : public TrafficFilter
    {
    public:
        MACsecFilter(
            _In_ const std::string &macsec_interface_name,
            _In_ int macsecfd);

        virtual ~MACsecFilter() = default;

        FilterStatus execute(
            _Inout_ void *buffer,
            _Inout_ ssize_t &length) override;

    protected:
        int m_macsecfd;
        const std::string m_macsec_interface_name;

        virtual FilterStatus forward(
            _In_ const void *buffer,
            _In_ ssize_t length) = 0;
    };

}  // namespace saivs
