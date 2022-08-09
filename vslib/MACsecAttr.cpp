#include "MACsecAttr.h"

#include "saimacsec.h"
#include "swss/logger.h"

#include <functional>

using namespace saivs;

const std::string MACsecAttr::CIPHER_NAME_INVALID = "";

const std::string MACsecAttr::CIPHER_NAME_GCM_AES_128 = "GCM-AES-128";

const std::string MACsecAttr::CIPHER_NAME_GCM_AES_256 = "GCM-AES-256";

const std::string MACsecAttr::CIPHER_NAME_GCM_AES_XPN_128 = "GCM-AES-XPN-128";

const std::string MACsecAttr::CIPHER_NAME_GCM_AES_XPN_256 = "GCM-AES-XPN-256";

const std::string MACsecAttr::DEFAULT_CIPHER_NAME = MACsecAttr::CIPHER_NAME_GCM_AES_128;

const macsec_an_t MACsecAttr::AN_INVALID = -1;

size_t MACsecAttr::Hash::operator()(const MACsecAttr &attr) const
{
    SWSS_LOG_ENTER();

    return std::hash<std::string>()(attr.m_macsecName) ^ std::hash<std::string>()(attr.m_sci) ^ attr.m_an;
}

MACsecAttr::MACsecAttr() : m_an(AN_INVALID)

{
    SWSS_LOG_ENTER();

    // empty intentionally
}

MACsecAttr::~MACsecAttr()
{
    SWSS_LOG_ENTER();

    // empty intentionally
}

const std::string & MACsecAttr::get_cipher_name(std::int32_t cipher_id)
{
    SWSS_LOG_ENTER();

    switch(cipher_id)
    {
        case sai_macsec_cipher_suite_t::SAI_MACSEC_CIPHER_SUITE_GCM_AES_128:
            return CIPHER_NAME_GCM_AES_128;

        case sai_macsec_cipher_suite_t::SAI_MACSEC_CIPHER_SUITE_GCM_AES_256:
            return CIPHER_NAME_GCM_AES_256;

        case sai_macsec_cipher_suite_t::SAI_MACSEC_CIPHER_SUITE_GCM_AES_XPN_128:
            return CIPHER_NAME_GCM_AES_XPN_128;

        case sai_macsec_cipher_suite_t::SAI_MACSEC_CIPHER_SUITE_GCM_AES_XPN_256:
            return CIPHER_NAME_GCM_AES_XPN_256;

        default:
            SWSS_LOG_ERROR("Unkown MACsec cipher %d", cipher_id);

            return CIPHER_NAME_INVALID;
    }
}

bool MACsecAttr::is_xpn() const
{
    SWSS_LOG_ENTER();

    return m_cipher == CIPHER_NAME_GCM_AES_XPN_128 || m_cipher == CIPHER_NAME_GCM_AES_XPN_256;
}

bool MACsecAttr::operator==(const MACsecAttr &attr) const
{
    SWSS_LOG_ENTER();

    return m_macsecName == attr.m_macsecName && m_sci == attr.m_sci && m_an == attr.m_an;
}
