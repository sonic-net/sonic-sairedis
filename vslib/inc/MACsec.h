#pragma once

#include <saitypes.h>

#include <string>
#include <cinttypes>

namespace saivs
{

using macsec_sci_t = std::uint64_t;
using macsec_an_t = std::uint16_t;
using macsec_pn_t = std::uint64_t;

struct MACsecSAAttr
{
    std::string veth_name;
    std::string macsec_name;
    macsec_sci_t sci;
    macsec_an_t an;
    macsec_pn_t pn;
    sai_int32_t direction;
    std::string sak;
    bool encryption_enable;
};

}
