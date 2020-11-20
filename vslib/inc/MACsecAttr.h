#pragma once

#include "HostInterfaceInfo.h"

#include <string>
#include <memory>

namespace saivs
{
    using macsec_sci_t = std::string;
    using macsec_an_t = std::uint16_t;
    using macsec_pn_t = std::uint64_t;

    struct MACsecAttr
    {
        // Explicitely declare constructor and destructor as non-inline functions
        // to avoid 'call is unlikely and code size would grow [-Werror=inline]'
        MACsecAttr();

        ~MACsecAttr();

        std::string m_vethName;
        std::string m_macsecName;
        std::string m_authKey;
        std::string m_sak;
        std::string m_sci;

        macsec_an_t m_an;
        macsec_pn_t m_pn;

        bool m_sendSci;
        bool m_encryptionEnable;

        sai_int32_t m_direction;

        std::shared_ptr<HostInterfaceInfo> m_info;
    };
}
