#pragma once

#include "HostInterfaceInfo.h"

#include <string>
#include <memory>

namespace saivs
{
    using macsec_sci_t = std::string;
    using macsec_an_t = std::uint16_t;
    using macsec_pn_t = std::uint64_t;
    using macsec_ssci_t = std::uint32_t;

    struct MACsecAttr
    {

        static const std::string CIPHER_NAME_INVALID;

        static const std::string CIPHER_NAME_GCM_AES_128;

        static const std::string CIPHER_NAME_GCM_AES_256;

        static const std::string CIPHER_NAME_GCM_AES_XPN_128;

        static const std::string CIPHER_NAME_GCM_AES_XPN_256;

        static const std::string DEFAULT_CIPHER_NAME;

        static const macsec_an_t AN_INVALID;

        struct Hash
        {
            size_t operator()(const MACsecAttr &attr) const;
        };

        // Explicitly declare constructor and destructor as non-inline functions
        // to avoid 'call is unlikely and code size would grow [-Werror=inline]'
        MACsecAttr();

        ~MACsecAttr();

        static const std::string &get_cipher_name(std::int32_t cipher_id);

        bool is_xpn() const;

        bool operator==(const MACsecAttr &attr) const;

        std::string m_cipher;
        std::string m_vethName;
        std::string m_macsecName;
        std::string m_authKey;
        std::string m_sak;
        std::string m_sci;
        std::string m_salt;

        macsec_an_t m_an;
        macsec_pn_t m_pn;
        macsec_ssci_t m_ssci;

        bool m_sendSci;
        bool m_encryptionEnable;

        sai_int32_t m_direction;

        std::shared_ptr<HostInterfaceInfo> m_info;
    };
}
