#pragma once

#include "TrafficFilter.h"

#include <string>

namespace saivs
{
    class MACsecFilter :
        public TrafficFilter
    {
        public:

            typedef enum _MACsecFilterState
            {
                MACSEC_FILTER_STATE_IDLE,

                MACSEC_FILTER_STATE_BUSY,

            } MACsecFilterState;

            MACsecFilter(
                    _In_ const std::string &macsecInterfaceName);

            virtual ~MACsecFilter() = default;

            virtual FilterStatus execute(
                    _Inout_ void *buffer,
                    _Inout_ size_t &length) override;

            void enable_macsec_device(
                    _In_ bool enable);

            void set_macsec_fd(
                    _In_ int macsecfd);

            MACsecFilterState get_state() const;

        protected:

            virtual FilterStatus forward(
                    _In_ const void *buffer,
                    _In_ size_t length) = 0;

            volatile bool m_macsecDeviceEnable;

            int m_macsecfd;

            const std::string m_macsecInterfaceName;

            MACsecFilterState m_state;
    };
}
