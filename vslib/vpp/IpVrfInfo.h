#pragma once

extern "C" {
#include "sai.h"
}

namespace saivs
{
    class IpVrfInfo
    {
        public:

            IpVrfInfo(
                    _In_ sai_object_id_t obj_id,
                    _In_ uint32_t vrf_id,
                    _In_ std::string &vrf_name,
                    _In_ bool is_ipv6);

            virtual ~IpVrfInfo();

        public:

            sai_object_id_t m_obj_id;

            uint32_t m_vrf_id;

            std::string m_vrf_name;

            bool m_is_ipv6;
    };
}
