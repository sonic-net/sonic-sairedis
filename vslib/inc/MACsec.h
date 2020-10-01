#pragma once

#include <saitypes.h>

#include <string>
#include <cinttypes>

namespace saivs
{

using macsec_sci_t = std::string;
using macsec_an_t = std::uint16_t;
using macsec_pn_t = std::uint64_t;

struct MACsecAttr
{
    std::string m_veth_name;
    std::string m_macsec_name;
    std::string m_sak;
    std::string m_sci;
    macsec_an_t m_an;
    macsec_pn_t m_pn;
    bool m_encryption_enable;
    sai_int32_t m_direction;
    ~MACsecAttr()
    {}
};

}
