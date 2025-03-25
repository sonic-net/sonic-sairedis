#pragma once

extern "C" {
#include "sai.h"
}

#include "swss/ipaddress.h"
#include "swss/ipprefix.h"

#include "vppxlate/SaiVppXlate.h"

#define IP_CMD "/sbin/ip"

#define PORTCHANNEL_PREFIX "PortChannel"

#define BONDETHERNET_PREFIX "BondEthernet"

#define CHECK_STATUS_W_MSG(status, msg, ...) {                                  \
    sai_status_t _status = (status);                            \
    if (_status != SAI_STATUS_SUCCESS) { \
        char buffer[512]; \
        snprintf(buffer, 512, msg, ##__VA_ARGS__); \
        SWSS_LOG_ERROR("%s: status %d", buffer, status); \
        return _status; } }

#define CHECK_STATUS_QUIET(status) {                                  \
    sai_status_t _status = (status);                            \
    if (_status != SAI_STATUS_SUCCESS) { return _status; } }

namespace saivs
{
    sai_status_t find_attrib_in_list(
            _In_ uint32_t                       attr_count,
            _In_ const sai_attribute_t         *attr_list,
            _In_ sai_attr_id_t                  attrib_id,
            _Out_ const sai_attribute_value_t **attr_value,
            _Out_ uint32_t                     *index);

    int getPrefixLenFromAddrMask(const uint8_t *addr, int len);

    swss::IpPrefix getIpPrefixFromSaiPrefix(const sai_ip_prefix_t& src);

    sai_ip_prefix_t& subnet(sai_ip_prefix_t& dst, const sai_ip_prefix_t& src);

    sai_ip_prefix_t& copy(sai_ip_prefix_t& dst, const swss::IpPrefix& src);

    void sai_ip_address_t_to_vpp_ip_addr_t(sai_ip_address_t& src, vpp_ip_addr_t& dst);

    /* Utility function for IP addr translation from VS to SAI */
    void vpp_ip_addr_t_to_sai_ip_address_t(vpp_ip_addr_t& src, sai_ip_address_t& dst);
}
