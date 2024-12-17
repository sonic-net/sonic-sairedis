#include "SwitchVppUtils.h"

#include "swss/logger.h"

#include "vppxlate/SaiVppXlate.h"

using namespace saivs;

sai_status_t saivs::find_attrib_in_list(
        _In_ uint32_t                       attr_count,
        _In_ const sai_attribute_t         *attr_list,
        _In_ sai_attr_id_t                  attrib_id,
        _Out_ const sai_attribute_value_t **attr_value,
        _Out_ uint32_t                     *index)
{
    SWSS_LOG_ENTER();

    uint32_t ii;

    if ((attr_count) && (NULL == attr_list)) {
        SWSS_LOG_ERROR("NULL value attr list\n");
        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (NULL == attr_value) {
        SWSS_LOG_ERROR("NULL value attr value\n");
        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (NULL == index) {
        SWSS_LOG_ERROR("NULL value index\n");
        return SAI_STATUS_INVALID_PARAMETER;
    }

    for (ii = 0; ii < attr_count; ii++) {
        if (attr_list[ii].id == attrib_id) {
            *attr_value = &(attr_list[ii].value);
            *index = ii;
            return SAI_STATUS_SUCCESS;
        }
    }

    *attr_value = NULL;

    return SAI_STATUS_ITEM_NOT_FOUND;
}

int saivs::getPrefixLenFromAddrMask(const uint8_t *addr, int len)
{
    SWSS_LOG_ENTER();

    // Iterate over each byte from left to right
    for (int i = 0; i < len; ++i)
    {
        uint8_t byte = addr[i];

        // If the byte is 0xFF, it means all bits are set, continue to the next byte
        if (byte == 0xFF)
        {
            continue;
        }

        // If the byte is not 0xFF, count the number of leading 1s in this byte
        int leading_ones = 0;
        for (int j = 7; j >= 0; --j)
        {
            if ((byte >> j) & 0x1)
            {
                ++leading_ones;
            }
            else
            {
                break;
            }
        }

        // Return the total number of leading 1s in the address mask
        return i * 8 + leading_ones;
    }

    // If all bytes are 0xFF, return the total length in bits
    return len * 8;
}

swss::IpPrefix saivs::getIpPrefixFromSaiPrefix(const sai_ip_prefix_t& src)
{
    SWSS_LOG_ENTER();

    swss::ip_addr_t ip;
    switch(src.addr_family)
    {
        case SAI_IP_ADDR_FAMILY_IPV4:
            ip.family = AF_INET;
            ip.ip_addr.ipv4_addr = src.addr.ip4;
            return swss::IpPrefix(ip, getPrefixLenFromAddrMask(reinterpret_cast<const uint8_t*>(&src.mask.ip4), 4));
        case SAI_IP_ADDR_FAMILY_IPV6:
            ip.family = AF_INET6;
            memcpy(ip.ip_addr.ipv6_addr, src.addr.ip6, 16);
            return swss::IpPrefix(ip, getPrefixLenFromAddrMask(src.mask.ip6, 16));
        default:
            throw std::logic_error("Invalid family");
    }
}

sai_ip_prefix_t& saivs::subnet(sai_ip_prefix_t& dst, const sai_ip_prefix_t& src)
{
    SWSS_LOG_ENTER();

    dst.addr_family = src.addr_family;
    switch(src.addr_family)
    {
        case SAI_IP_ADDR_FAMILY_IPV4:
            dst.addr.ip4 = src.addr.ip4 & src.mask.ip4;
            dst.mask.ip4 = src.mask.ip4;
            break;
        case SAI_IP_ADDR_FAMILY_IPV6:
            for (size_t i = 0; i < 16; i++)
            {
                dst.addr.ip6[i] = src.addr.ip6[i] & src.mask.ip6[i];
                dst.mask.ip6[i] = src.mask.ip6[i];
            }
            break;
        default:
            throw std::logic_error("Invalid family");
    }
    return dst;
}

sai_ip_prefix_t& saivs::copy(sai_ip_prefix_t& dst, const swss::IpPrefix& src)
{
    SWSS_LOG_ENTER();

    auto ia = src.getIp().getIp();
    auto ma = src.getMask().getIp();
    switch(ia.family)
    {
        case AF_INET:
            dst.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
            dst.addr.ip4 = ia.ip_addr.ipv4_addr;
            dst.mask.ip4 = ma.ip_addr.ipv4_addr;
            break;
        case AF_INET6:
            dst.addr_family = SAI_IP_ADDR_FAMILY_IPV6;
            memcpy(dst.addr.ip6, ia.ip_addr.ipv6_addr, 16);
            memcpy(dst.mask.ip6, ma.ip_addr.ipv6_addr, 16);
            break;
        default:
            throw std::logic_error("Invalid family");
    }
    return dst;
}

void saivs::sai_ip_address_t_to_vpp_ip_addr_t(sai_ip_address_t& src, vpp_ip_addr_t& dst)
{
    SWSS_LOG_ENTER();

    if (SAI_IP_ADDR_FAMILY_IPV4 == src.addr_family) {
        struct sockaddr_in *sin =  &dst.addr.ip4;

        dst.sa_family = AF_INET;
        sin->sin_family = AF_INET;
        sin->sin_addr.s_addr = src.addr.ip4;
    } else {
        struct sockaddr_in6 *sin6 =  &dst.addr.ip6;

        dst.sa_family = AF_INET6;
        sin6->sin6_family = AF_INET6;
        memcpy(sin6->sin6_addr.s6_addr, src.addr.ip6, sizeof(sin6->sin6_addr.s6_addr));
    }
}

/* Utility function for IP addr translation from VS to SAI */
void saivs::vpp_ip_addr_t_to_sai_ip_address_t(vpp_ip_addr_t& src, sai_ip_address_t& dst)
{
    SWSS_LOG_ENTER();

    if (src.sa_family == AF_INET)
    {
        dst.addr_family = SAI_IP_ADDR_FAMILY_IPV4;
        struct sockaddr_in* sin = &src.addr.ip4;
        memcpy(&dst.addr.ip4, &sin->sin_addr.s_addr,
                sizeof(sin->sin_addr.s_addr));
    }
    else {
        dst.addr_family = SAI_IP_ADDR_FAMILY_IPV6;
        struct sockaddr_in6* sin6 = &src.addr.ip6;
        memcpy(&dst.addr.ip6, sin6->sin6_addr.s6_addr,
                sizeof(sin6->sin6_addr.s6_addr));
    }
}
