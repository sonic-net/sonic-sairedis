#include "SwitchVpp.h"
#include "SwitchVppAcl.h"
#include "SwitchVppUtils.h"

#include "meta/sai_serialize.h"

#include "swss/logger.h"
#include "swss/exec.h"
#include "swss/converter.h"

#include "vppxlate/SaiVppXlate.h"
#include "vppxlate/SaiAclStats.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>

#include <list>
#include <vector>
#include <algorithm>

using namespace saivs;

#define DEFAULT_PERMIT_RULES 2

static sai_status_t acl_ip_field_to_vpp_acl(
    _In_ sai_acl_entry_attr_t         attr_id,
    _In_ const sai_attribute_value_t *value,
    _Out_ vpp_acl_rule_t *rule)
{
    SWSS_LOG_ENTER();

    sai_ip_addr_family_t                    addr_family;

    assert((SAI_ACL_ENTRY_ATTR_FIELD_SRC_IPV6 == attr_id) ||
           (SAI_ACL_ENTRY_ATTR_FIELD_DST_IPV6 == attr_id) ||
           (SAI_ACL_ENTRY_ATTR_FIELD_SRC_IP == attr_id) ||
           (SAI_ACL_ENTRY_ATTR_FIELD_DST_IP == attr_id) ||
           (SAI_ACL_ENTRY_ATTR_FIELD_INNER_SRC_IP == attr_id) ||
           (SAI_ACL_ENTRY_ATTR_FIELD_INNER_DST_IP == attr_id) ||
           (SAI_ACL_ENTRY_ATTR_FIELD_INNER_SRC_IPV6 == attr_id) ||
           (SAI_ACL_ENTRY_ATTR_FIELD_INNER_DST_IPV6 == attr_id));

    if (!value->aclfield.enable) {
        SWSS_LOG_INFO("aclfield not enabled for ip prefix");
        return SAI_STATUS_SUCCESS;
    }

    vpp_ip_addr_t *ip_addr, *ip_mask;

    switch (attr_id) {
    case SAI_ACL_ENTRY_ATTR_FIELD_SRC_IP:
    case SAI_ACL_ENTRY_ATTR_FIELD_INNER_SRC_IP:
        ip_addr = &rule->src_prefix;
        ip_mask = &rule->src_prefix_mask;
        addr_family = SAI_IP_ADDR_FAMILY_IPV4;
        break;

    case SAI_ACL_ENTRY_ATTR_FIELD_DST_IP:
    case SAI_ACL_ENTRY_ATTR_FIELD_INNER_DST_IP:
        ip_addr = &rule->dst_prefix;
        ip_mask = &rule->dst_prefix_mask;
        addr_family = SAI_IP_ADDR_FAMILY_IPV4;
        break;

    case SAI_ACL_ENTRY_ATTR_FIELD_SRC_IPV6:
    case SAI_ACL_ENTRY_ATTR_FIELD_INNER_SRC_IPV6:
        ip_addr = &rule->src_prefix;
        ip_mask = &rule->src_prefix_mask;
        addr_family = SAI_IP_ADDR_FAMILY_IPV6;
        break;

    case SAI_ACL_ENTRY_ATTR_FIELD_DST_IPV6:
    case SAI_ACL_ENTRY_ATTR_FIELD_INNER_DST_IPV6:
        ip_addr = &rule->dst_prefix;
        ip_mask = &rule->dst_prefix_mask;
        addr_family = SAI_IP_ADDR_FAMILY_IPV6;
        break;

    default:
        SWSS_LOG_ERROR("Unexpected ip field type (%u)\n", attr_id);
        return SAI_STATUS_FAILURE;
    }

    if (SAI_IP_ADDR_FAMILY_IPV4 == addr_family) {
        struct sockaddr_in *sin =  &ip_addr->addr.ip4;

        ip_addr->sa_family = AF_INET;
        sin->sin_addr.s_addr = value->aclfield.data.ip4;

        sin =  &ip_mask->addr.ip4;
        sin->sin_addr.s_addr = value->aclfield.mask.ip4;
        SWSS_LOG_INFO("Setting ipv4 subnet %x %x", value->aclfield.data.ip4,
                        value->aclfield.mask.ip4);
    } else {
        struct sockaddr_in6 *sin6 =  &ip_addr->addr.ip6;

        ip_addr->sa_family = AF_INET6;
        memcpy(sin6->sin6_addr.s6_addr, value->aclfield.data.ip6, sizeof(sin6->sin6_addr.s6_addr));

        sin6 =  &ip_mask->addr.ip6;
        memcpy(sin6->sin6_addr.s6_addr, &value->aclfield.mask.ip6, sizeof(value->aclfield.mask.ip6));
    }

    return SAI_STATUS_SUCCESS;
}

static void set_ipv4any_addr_mask (vpp_ip_addr_t *ip_addr)
{
    SWSS_LOG_ENTER();

    struct sockaddr_in *sin =  &ip_addr->addr.ip4;

    ip_addr->sa_family = AF_INET;
    sin->sin_addr.s_addr = (uint32_t) 0;
}

static void set_ipv6any_addr_mask (vpp_ip_addr_t *ip_addr)
{
    SWSS_LOG_ENTER();

    struct sockaddr_in6 *sin6 =  &ip_addr->addr.ip6;

    ip_addr->sa_family = AF_INET6;

    unsigned long long v6_mask = (unsigned long long) 0;

    memcpy(sin6->sin6_addr.s6_addr, &v6_mask, 8);
    memcpy(&sin6->sin6_addr.s6_addr[8], &v6_mask, 8);
}

static sai_status_t acl_ip_type_field_to_vpp_acl_rule(
    _In_ sai_acl_entry_attr_t         attr_id,
    _In_ const sai_attribute_value_t *value,
    _Out_ vpp_acl_rule_t *rule)
{
    SWSS_LOG_ENTER();


    sai_acl_ip_type_t ip_type;

    assert(SAI_ACL_ENTRY_ATTR_FIELD_ACL_IP_TYPE == attr_id);

    if (!value->aclfield.enable) {
        return SAI_STATUS_SUCCESS;
    }

    ip_type = (sai_acl_ip_type_t) value->aclfield.data.s32;

    switch (ip_type) {
    case SAI_ACL_IP_TYPE_ANY:
        /* Do nothing */
        break;

    case SAI_ACL_IP_TYPE_IP:
        /* Do nothing for now */
        break;

    case SAI_ACL_IP_TYPE_IPV4ANY:
        set_ipv4any_addr_mask(&rule->src_prefix);
        set_ipv4any_addr_mask(&rule->dst_prefix);
        set_ipv4any_addr_mask(&rule->src_prefix_mask);
        set_ipv4any_addr_mask(&rule->dst_prefix_mask);

        break;

    case SAI_ACL_IP_TYPE_IPV6ANY:
        set_ipv6any_addr_mask(&rule->src_prefix);
        set_ipv6any_addr_mask(&rule->dst_prefix);
        set_ipv6any_addr_mask(&rule->src_prefix_mask);
        set_ipv6any_addr_mask(&rule->dst_prefix_mask);

        break;

    default:
        SWSS_LOG_INFO("Unsupported ip type (%d)\n", ip_type);
        return SAI_STATUS_SUCCESS;
    }
    return SAI_STATUS_SUCCESS;
}

static sai_status_t acl_icmp_field_to_vpp_acl_rule(
    _In_ sai_acl_entry_attr_t          attr_id,
    _In_ const sai_attribute_value_t  *value,
    _Out_ vpp_acl_rule_t *rule)
{
    SWSS_LOG_ENTER();

    uint16_t                                first = 0, last = 0;
    uint16_t                                new_data, new_mask;

    assert((SAI_ACL_ENTRY_ATTR_FIELD_ICMP_CODE == attr_id) ||
           (SAI_ACL_ENTRY_ATTR_FIELD_ICMP_TYPE == attr_id) ||
           (SAI_ACL_ENTRY_ATTR_FIELD_ICMPV6_CODE == attr_id) ||
           (SAI_ACL_ENTRY_ATTR_FIELD_ICMPV6_TYPE == attr_id));

    new_data = (value->aclfield.enable) ? value->aclfield.data.u8 : 0;
    new_mask = (value->aclfield.enable) ? value->aclfield.mask.u8 : 0;

    first = new_data & new_mask;
    last = new_data | (~new_mask & 0xFF);

    switch (attr_id) {
    case SAI_ACL_ENTRY_ATTR_FIELD_ICMP_CODE:
    case SAI_ACL_ENTRY_ATTR_FIELD_ICMPV6_CODE:
        rule->dstport_or_icmpcode_first = first;
        rule->dstport_or_icmpcode_last = last;

        break;

    case SAI_ACL_ENTRY_ATTR_FIELD_ICMP_TYPE:
    case SAI_ACL_ENTRY_ATTR_FIELD_ICMPV6_TYPE:
        rule->srcport_or_icmptype_first = first;
        rule->srcport_or_icmptype_last = last;

        break;

    default:
        SWSS_LOG_ERROR("Unexpected attr_id %d\n", attr_id);
        return SAI_STATUS_FAILURE;
    }

    return SAI_STATUS_SUCCESS;
}

static sai_status_t acl_entry_port_to_vpp_acl_rule(
    _In_ sai_acl_entry_attr_t          attr_id,
    _In_ const sai_attribute_value_t  *value,
    _Out_ vpp_acl_rule_t      *rule)
{
    SWSS_LOG_ENTER();

    if (!value->aclfield.enable) {
        SWSS_LOG_INFO("aclfield disabled for port configuration");
        return SAI_STATUS_SUCCESS;
    }

    switch (attr_id) {
    case SAI_ACL_ENTRY_ATTR_FIELD_L4_SRC_PORT:
        rule->srcport_or_icmptype_first = value->aclfield.data.u16;
        rule->srcport_or_icmptype_last = value->aclfield.data.u16;
        break;

    case SAI_ACL_ENTRY_ATTR_FIELD_L4_DST_PORT:
        rule->dstport_or_icmpcode_first = value->aclfield.data.u16;
        rule->dstport_or_icmpcode_last = value->aclfield.data.u16;
        break;

    default:
        break;
    }
    return SAI_STATUS_SUCCESS;
}

static sai_status_t acl_rule_port_range_vpp_acl_set(
    _In_ sai_acl_range_type_t     type,
    _In_ const sai_u32_range_t   *range,
    _Out_ vpp_acl_rule_t *rule)
{
    SWSS_LOG_ENTER();

    assert(range);

    switch (type) {
    case SAI_ACL_RANGE_TYPE_L4_SRC_PORT_RANGE:
        if (rule->proto != 0 && rule->proto != IPPROTO_TCP && rule->proto != IPPROTO_UDP) {
            SWSS_LOG_ERROR(
                "Conflicting protocol settings: "
                "src port range requires TCP/UDP, but proto is already set to %u",
                rule->proto);
            return SAI_STATUS_FAILURE;
        }
        rule->srcport_or_icmptype_first = (uint16_t) range->min;
        rule->srcport_or_icmptype_last = (uint16_t) range->max;
        break;

    case SAI_ACL_RANGE_TYPE_L4_DST_PORT_RANGE:
        if (rule->proto != 0 && rule->proto != IPPROTO_TCP) {
            SWSS_LOG_ERROR(
                "Conflicting protocol settings: "
                "dst port range requires TCP/UDP, but proto is already set to %u",
                rule->proto);
            return SAI_STATUS_FAILURE;
        }
        rule->dstport_or_icmpcode_first = (uint16_t) range->min;
        rule->dstport_or_icmpcode_last = (uint16_t) range->max;
        break;

    default:
        SWSS_LOG_INFO("Range type %d is not supported\n", type);
        break;
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::acl_rule_range_get(
    _In_ const sai_object_list_t   *range_list,
    _Out_ sai_u32_range_t *range_limit_list,
    _Out_ sai_acl_range_type_t *range_type_list,
    _Out_ uint32_t *range_count)
{
    SWSS_LOG_ENTER();

    uint32_t idx, count = 0;

    sai_u32_range_t   *range = range_limit_list;
    sai_acl_range_type_t *range_type = range_type_list;

    for (idx = 0; idx < range_list->count; idx++) {
        sai_object_id_t oid;

        oid = range_list->list[idx];

        if (SAI_OBJECT_TYPE_ACL_RANGE == RealObjectIdManager::objectTypeQuery(oid)) {
            sai_attribute_t attr;

            attr.id = SAI_ACL_RANGE_ATTR_TYPE;
            if (get(SAI_OBJECT_TYPE_ACL_RANGE, oid, 1, &attr) == SAI_STATUS_SUCCESS) {
                sai_acl_range_type_t     type;

                type = (sai_acl_range_type_t) attr.value.s32;
                attr.id = SAI_ACL_RANGE_ATTR_LIMIT;
                if (get(SAI_OBJECT_TYPE_ACL_RANGE, oid, 1, &attr) == SAI_STATUS_SUCCESS) {

                    *range = attr.value.u32range;
                    *range_type = type;

                    range++;
                    range_type++;
                    count++;

                    if (count == 2) break;
                }
            } else {
                SWSS_LOG_ERROR("SAI_OBJECT_TYPE_ACL_RANGE not found for ACL_RANGE oid");
                return SAI_STATUS_FAILURE;
            }
        }
    }

    *range_count = count;

    return SAI_STATUS_SUCCESS;
}

static void acl_rule_set_action(
    _In_ const sai_attribute_value_t  *value,
    _Out_ vpp_acl_rule_t      *rule)

{
    SWSS_LOG_ENTER();

    switch (value->aclaction.parameter.s32) {
        case SAI_PACKET_ACTION_FORWARD:
            rule->action = VPP_ACL_ACTION_API_PERMIT;
            break;

        case SAI_PACKET_ACTION_DROP:
            rule->action = VPP_ACL_ACTION_API_DENY;
            break;
        }
}

sai_status_t acl_rule_field_update(
    _In_ sai_acl_entry_attr_t          attr_id,
    _In_ const sai_attribute_value_t  *value,
    _Out_ vpp_acl_rule_t      *rule)
{
    SWSS_LOG_ENTER();

    sai_status_t status;

    assert(NULL != value);
    status = SAI_STATUS_SUCCESS;

    switch (attr_id) {
    case SAI_ACL_ENTRY_ATTR_FIELD_SRC_IP:
    case SAI_ACL_ENTRY_ATTR_FIELD_DST_IP:
    case SAI_ACL_ENTRY_ATTR_FIELD_SRC_IPV6:
    case SAI_ACL_ENTRY_ATTR_FIELD_DST_IPV6:
    case SAI_ACL_ENTRY_ATTR_FIELD_INNER_SRC_IP:
    case SAI_ACL_ENTRY_ATTR_FIELD_INNER_DST_IP:
    case SAI_ACL_ENTRY_ATTR_FIELD_INNER_SRC_IPV6:
    case SAI_ACL_ENTRY_ATTR_FIELD_INNER_DST_IPV6:
        status = acl_ip_field_to_vpp_acl(attr_id, value, rule);
        break;

    case SAI_ACL_ENTRY_ATTR_FIELD_ACL_IP_TYPE:
        status = acl_ip_type_field_to_vpp_acl_rule(attr_id, value, rule);
        break;

    case SAI_ACL_ENTRY_ATTR_FIELD_ICMP_CODE:
    case SAI_ACL_ENTRY_ATTR_FIELD_ICMP_TYPE:
        if (rule->proto != 0 && rule->proto != IPPROTO_ICMP) {
            SWSS_LOG_ERROR(
                "Conflicting protocol settings: "
                "ICMP requires ICMP protocol, but proto is already set to %u",
                rule->proto);
            return SAI_STATUS_FAILURE;
        }
        rule->proto = IPPROTO_ICMP;
        status = acl_icmp_field_to_vpp_acl_rule(attr_id, value, rule);
        break;

    case SAI_ACL_ENTRY_ATTR_FIELD_ICMPV6_CODE:
    case SAI_ACL_ENTRY_ATTR_FIELD_ICMPV6_TYPE:
        if (rule->proto != 0 && rule->proto != IPPROTO_ICMPV6) {
            SWSS_LOG_ERROR(
                "Conflicting protocol settings: "
                "ICMPv6 requires ICMPv6 protocol, but proto is already set to %u",
                rule->proto);
            return SAI_STATUS_FAILURE;
        }
        rule->proto = IPPROTO_ICMPV6;
        status = acl_icmp_field_to_vpp_acl_rule(attr_id, value, rule);
        break;

    case SAI_ACL_ENTRY_ATTR_FIELD_L4_SRC_PORT:
    case SAI_ACL_ENTRY_ATTR_FIELD_L4_DST_PORT:
        if (rule->proto != 0 && rule->proto != IPPROTO_TCP && rule->proto != IPPROTO_UDP) {
            SWSS_LOG_ERROR(
                "Conflicting protocol settings: "
                "src/dst port requires TCP/UDP, but proto is already set to %u",
                rule->proto);
            return SAI_STATUS_FAILURE;
        }
        status = acl_entry_port_to_vpp_acl_rule(attr_id, value, rule);
        break;

    case SAI_ACL_ENTRY_ATTR_FIELD_IP_PROTOCOL:
        rule->proto = value->aclfield.data.u8 & value->aclfield.mask.u8;
        status = SAI_STATUS_SUCCESS;
        break;

    case SAI_ACL_ENTRY_ATTR_FIELD_TCP_FLAGS:
        if (rule->proto != 0 && rule->proto != IPPROTO_TCP) {
            SWSS_LOG_ERROR(
                "Conflicting protocol settings: "
                "TCP flags require TCP, but proto is already set to %u",
                rule->proto);
            return SAI_STATUS_FAILURE;
        }
        rule->proto = IPPROTO_TCP;
        rule->tcp_flags_mask = value->aclfield.mask.u8;
        rule->tcp_flags_value = value->aclfield.data.u8;
        break;

    case SAI_ACL_ENTRY_ATTR_FIELD_IPV6_NEXT_HEADER:
        if (rule->proto != 0 && rule->proto != (value->aclfield.data.u8 & value->aclfield.mask.u8)) {
            SWSS_LOG_ERROR(
                "Conflicting protocol settings: "
                "IPV6_NEXT_HEADER specified but proto is already set to %u",
                rule->proto);
            return SAI_STATUS_FAILURE;
        }
        rule->proto = value->aclfield.data.u8 & value->aclfield.mask.u8;
        break;

    case SAI_ACL_ENTRY_ATTR_ACTION_PACKET_ACTION:
        acl_rule_set_action(value, rule);
        break;

    case SAI_ACL_ENTRY_ATTR_PRIORITY:
    case SAI_ACL_ENTRY_ATTR_TABLE_ID:
    case SAI_ACL_ENTRY_ATTR_ADMIN_STATE:
    case SAI_ACL_TABLE_ATTR_FIELD_ETHER_TYPE:
    case SAI_ACL_ENTRY_ATTR_ACTION_COUNTER:
        // NOOP here - these are either handled elsewhere or not currently applicable
        break;

    default:
        SWSS_LOG_ERROR("Unhandled ACL entry attribute ID: %d", attr_id);
        break;
    }

    return status;
}

sai_status_t SwitchVpp::tunterm_set_action_redirect(
    _In_ sai_acl_entry_attr_t          attr_id,
    _In_ const sai_attribute_value_t  *value,
    _Out_ vpp_tunterm_acl_rule_t      *rule)
{
    SWSS_LOG_ENTER();

    sai_status_t                 status = SAI_STATUS_SUCCESS;
    sai_object_id_t              next_hop_oid;
    sai_object_id_t              rif_oid;
    sai_object_id_t              port_oid;
    sai_ip_address_t             ip_address;
    uint32_t                     next_hop_type;
    uint16_t                     vlan_id = 0;
    char                         nh_ip_str[INET6_ADDRSTRLEN];

    next_hop_oid = value->aclaction.parameter.oid;

    sai_attribute_t attr;
    attr.id = SAI_NEXT_HOP_ATTR_TYPE;
    CHECK_STATUS(get(SAI_OBJECT_TYPE_NEXT_HOP, next_hop_oid, 1, &attr));
    next_hop_type = attr.value.s32;
    if (next_hop_type!= SAI_NEXT_HOP_TYPE_IP) {
        return SAI_STATUS_SUCCESS;
    }
    attr.id = SAI_NEXT_HOP_ATTR_IP;
    if (get(SAI_OBJECT_TYPE_NEXT_HOP, next_hop_oid, 1, &attr) == SAI_STATUS_SUCCESS)
    {
        ip_address = attr.value.ipaddr;
    } else {
        SWSS_LOG_ERROR("IP address missing in nexthop %s", sai_serialize_object_id(next_hop_oid).c_str());
        return SAI_STATUS_SUCCESS;
    }

    attr.id = SAI_NEXT_HOP_ATTR_ROUTER_INTERFACE_ID;
    if (get(SAI_OBJECT_TYPE_NEXT_HOP, next_hop_oid, 1, &attr) == SAI_STATUS_SUCCESS)
    {
        rif_oid = attr.value.oid;
    } else {
        SWSS_LOG_ERROR("RIF missing in nexthop %s", sai_serialize_object_id(next_hop_oid).c_str());
        return SAI_STATUS_SUCCESS;
    }

    attr.id = SAI_ROUTER_INTERFACE_ATTR_PORT_ID;
    if (get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, rif_oid, 1, &attr) == SAI_STATUS_SUCCESS)
    {
        port_oid = attr.value.oid;
    } else {
        SWSS_LOG_ERROR("Port ID is missing missing in RIF object %s", sai_serialize_object_id(rif_oid).c_str());
        return SAI_STATUS_SUCCESS;
    }

    attr.id = SAI_ROUTER_INTERFACE_ATTR_OUTER_VLAN_ID;
    if (get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, rif_oid, 1, &attr) == SAI_STATUS_SUCCESS)
    {
        vlan_id = attr.value.u16;
    }

    std::string hwif_name;
    if (!vpp_get_hwif_name(port_oid, vlan_id, hwif_name)) {
        SWSS_LOG_WARN("VS hwif name not found for port %s", sai_serialize_object_id(port_oid).c_str());
        return SAI_STATUS_SUCCESS;
    }

    switch (ip_address.addr_family) {
        case SAI_IP_ADDR_FAMILY_IPV4:
        {
            sai_ip_address_t_to_vpp_ip_addr_t(ip_address, rule->next_hop_ip);
            rule->ip_protocol = 1; // IP46_TYPE_IP4=1, IP46_TYPE_IP6=2
            break;
        }
        case SAI_IP_ADDR_FAMILY_IPV6:
        {
            sai_ip_address_t_to_vpp_ip_addr_t(ip_address, rule->next_hop_ip);
            rule->ip_protocol = 2; // IP46_TYPE_IP4=1, IP46_TYPE_IP6=2
            break;
        }
        default:
            break;
    }

    strncpy(rule->hwif_name, hwif_name.c_str(), sizeof(rule->hwif_name) -1);

    SWSS_LOG_INFO("Tunterm rule received: IP Protocol %d, rif_oid %ld, next-hop hwif_name %s", rule->ip_protocol,
                    rif_oid, rule->hwif_name);

    vpp_ip_addr_t_to_string(&rule->next_hop_ip, nh_ip_str, INET6_ADDRSTRLEN);
    SWSS_LOG_INFO("Tunterm acl rule has next-hop IP %s", nh_ip_str);

    return status;
}

sai_status_t SwitchVpp::tunterm_acl_rule_field_update(
    _In_ sai_acl_entry_attr_t          attr_id,
    _In_ const sai_attribute_value_t  *value,
    _Out_ vpp_tunterm_acl_rule_t      *rule)
{
    SWSS_LOG_ENTER();

    sai_status_t status;
    char         dst_ip_str[INET6_ADDRSTRLEN];

    assert(NULL != value);
    status = SAI_STATUS_SUCCESS;

    switch (attr_id) {
    case SAI_ACL_ENTRY_ATTR_FIELD_DST_IP:
    case SAI_ACL_ENTRY_ATTR_FIELD_INNER_DST_IP:
    {
        vpp_ip_addr_t *ip_addr, *ip_mask;
        ip_addr = &rule->dst_prefix;
        ip_mask = &rule->dst_prefix_mask;
        struct sockaddr_in *sin =  &ip_addr->addr.ip4;
        ip_addr->sa_family = AF_INET;
        sin->sin_addr.s_addr = value->aclfield.data.ip4;
        sin =  &ip_mask->addr.ip4;
        sin->sin_addr.s_addr = value->aclfield.mask.ip4;
        vpp_ip_addr_t_to_string(ip_addr, dst_ip_str, INET6_ADDRSTRLEN);
        SWSS_LOG_INFO("Tunterm acl rule has dst IP: %s", dst_ip_str);
        break;
    }
    case SAI_ACL_ENTRY_ATTR_FIELD_DST_IPV6:
    case SAI_ACL_ENTRY_ATTR_FIELD_INNER_DST_IPV6:
    {
        vpp_ip_addr_t *ip_addr, *ip_mask;
        ip_addr = &rule->dst_prefix;
        ip_mask = &rule->dst_prefix_mask;
        struct sockaddr_in6 *sin6 =  &ip_addr->addr.ip6;
        ip_addr->sa_family = AF_INET6;
        memcpy(sin6->sin6_addr.s6_addr, value->aclfield.data.ip6, sizeof(sin6->sin6_addr.s6_addr));
        sin6 =  &ip_mask->addr.ip6;
        memcpy(sin6->sin6_addr.s6_addr, &value->aclfield.mask.ip6, sizeof(value->aclfield.mask.ip6));
        vpp_ip_addr_t_to_string(ip_addr, dst_ip_str, INET6_ADDRSTRLEN);
        SWSS_LOG_INFO("Tunterm acl rule has dst IP: %s", dst_ip_str);
        break;
    }
    case SAI_ACL_ENTRY_ATTR_ACTION_REDIRECT:
        status = tunterm_set_action_redirect(attr_id, value, rule);
        break;
    default:
        break;
    }

    return status;
}

sai_status_t SwitchVpp::getAclTableId(
    _In_ sai_object_id_t entry_id, sai_object_id_t *tbl_oid)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    attr.id = SAI_ACL_ENTRY_ATTR_TABLE_ID;
    if (get(SAI_OBJECT_TYPE_ACL_ENTRY, entry_id, 1, &attr) != SAI_STATUS_SUCCESS) {
        auto sid = sai_serialize_object_id(entry_id);

        SWSS_LOG_ERROR("ACL table for acl entry id %s not found", sid.c_str());
        return SAI_STATUS_FAILURE;
    }

    *tbl_oid = attr.value.oid;

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::acl_range_attr_get (
    _In_ const std::string &serializedObjectId,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list,
    _Out_ sai_attribute_t *attr_range)
{
    SWSS_LOG_ENTER();

    const sai_attribute_t *attr;

    for (uint32_t i = 0; i < attr_count; i++) {
        attr = &attr_list[i];
        if (attr->id == SAI_ACL_ENTRY_ATTR_FIELD_ACL_RANGE_TYPE) {
            attr_range->id = SAI_ACL_ENTRY_ATTR_FIELD_ACL_RANGE_TYPE;
            return get(SAI_OBJECT_TYPE_ACL_ENTRY, serializedObjectId,
                       1, attr_range);
        }
    }

    return SAI_STATUS_FAILURE;
}

sai_status_t acl_priority_attr_get (
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list,
    uint32_t *priority)
{
    SWSS_LOG_ENTER();

    const sai_attribute_t *attr;

    for (uint32_t i = 0; i < attr_count; i++) {
        attr = &attr_list[i];
        if (attr->id == SAI_ACL_ENTRY_ATTR_PRIORITY) {
            *priority = attr->value.u32;
            return SAI_STATUS_SUCCESS;
        }
    }

    return SAI_STATUS_FAILURE;
}

static bool cmp_priority (
    const ordered_ace_list_t& f,
    const ordered_ace_list_t& s)
{
    SWSS_LOG_ENTER();

    return (f.priority > s.priority);
}

sai_status_t SwitchVpp::get_sorted_aces(
    sai_object_id_t tbl_oid,
    size_t &n_total_entries,
    acl_tbl_entries_t *&aces,
    std::list<ordered_ace_list_t> &ordered_aces)
{
    SWSS_LOG_ENTER();

    sai_status_t                status = SAI_STATUS_SUCCESS;
    auto                        it = m_acl_tbl_rules_map.find(tbl_oid);

    if (it == m_acl_tbl_rules_map.end()) {
        auto sid = sai_serialize_object_id(tbl_oid);
        SWSS_LOG_INFO("No ACL entry list for table id %s", sid.c_str());
        return status;
    }

    n_total_entries = it->second.size();
    if (n_total_entries == 0) {
        return SAI_STATUS_SUCCESS;
    }

    std::list<sai_object_id_t> &acl_entries = it->second;
    acl_tbl_entries_t          *p_ace = NULL;
    aces = (acl_tbl_entries_t *) calloc(n_total_entries, sizeof(acl_tbl_entries_t));
    if (!aces) {
        SWSS_LOG_ERROR("Failed to allocate memory for aces.");
        return SAI_STATUS_FAILURE;
    }
    p_ace = aces;

    /* Collect ACL entries configuration */
    uint32_t index = 0;
    for (auto entry_id: acl_entries) {
        SWSS_LOG_INFO("Processing ACL entry %s", sai_serialize_object_id(entry_id).c_str());

        auto sid = sai_serialize_object_id(entry_id);

        if (get_max(SAI_OBJECT_TYPE_ACL_ENTRY, sid, MAX_ACL_ATTRS,
            &p_ace->attrs_count, p_ace->attrs) != SAI_STATUS_SUCCESS) {
            SWSS_LOG_ERROR("Failed to get acl entry.");
            status = SAI_STATUS_FAILURE;
            break;
        }
        p_ace->attr_range.value.aclfield.data.objlist.list = p_ace->range_objid_list;
        p_ace->attr_range.value.aclfield.data.objlist.count = 2;

        if (acl_range_attr_get(sid, p_ace->attrs_count,
                        p_ace->attrs, &p_ace->attr_range) == SAI_STATUS_SUCCESS) {
            p_ace->range_count = 0;
            if (acl_rule_range_get(&p_ace->attr_range.value.aclfield.data.objlist,
                p_ace->range_limit, p_ace->range_type, &p_ace->range_count)
                != SAI_STATUS_SUCCESS) {
                SWSS_LOG_ERROR("Failed acl range get.");
                status = SAI_STATUS_FAILURE;
                break;
            }
        }

        p_ace->priority = 0;
        acl_priority_attr_get(p_ace->attrs_count, p_ace->attrs, &p_ace->priority);

        ordered_aces.push_back({index, p_ace->priority, entry_id, false, 0, 0});
        p_ace++;
        index++;
    }

    if (status != SAI_STATUS_SUCCESS) {
        free(aces);
        ordered_aces.clear();
        return SAI_STATUS_FAILURE;
    }

    /* Sort ACL entries on priority */
    ordered_aces.sort(cmp_priority);

    return SAI_STATUS_SUCCESS;
}

void SwitchVpp::count_tunterm_acl_rules(
    acl_tbl_entries_t *aces,
    std::list<ordered_ace_list_t> &ordered_aces,
    size_t &n_entries,
    size_t &n_tunterm_entries)
{
    SWSS_LOG_ENTER();

    acl_tbl_entries_t     *p_ace = NULL;
    const sai_attribute_t *attr = NULL;
    bool                   tunterm_flag_set = false;

    for (auto &ace: ordered_aces) {
        p_ace = &aces[ace.index];
        tunterm_flag_set = false;
        for (uint32_t i = 0; i < p_ace->attrs_count; i++) {
            attr = &p_ace->attrs[i];
            if (attr->id == SAI_ACL_ENTRY_ATTR_FIELD_TUNNEL_TERMINATED) {
                if (attr->value.aclfield.data.booldata == true) {
                    tunterm_flag_set = true;
                }
                break;
            }
        }
        if (tunterm_flag_set) {
            n_tunterm_entries++;
            ace.is_tunterm = true;
        } else {
            n_entries++;
        }
    }
}

sai_status_t SwitchVpp::fill_acl_rules(
    acl_tbl_entries_t *aces,
    std::list<ordered_ace_list_t> &ordered_aces,
    std::list<vpp_acl_rule_t> &acl_rules,
    std::list<vpp_tunterm_acl_rule_t> &tunterm_acl_rules)
{
    SWSS_LOG_ENTER();

    sai_status_t status = SAI_STATUS_SUCCESS;
    acl_tbl_entries_t *p_ace = NULL;
    uint32_t acl_rule_index = 0;
    uint32_t tunterm_rule_index = 0;

    for (auto &ace: ordered_aces) {
        SWSS_LOG_INFO("Acl entry index %u priority %u", ace.index, ace.priority);
        p_ace = &aces[ace.index];
        const sai_attribute_t *attr;

        if (ace.is_tunterm) {
            // Process tunnel termination ACL rule
            vpp_tunterm_acl_rule_t tunterm_rule = {};

            // Record the base index for this ACE
            ace.vpp_rule_base_index = tunterm_rule_index;

            for (uint32_t i = 0; i < p_ace->attrs_count && status == SAI_STATUS_SUCCESS; i++) {
                attr = &p_ace->attrs[i];
                auto meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_ACL_ENTRY, attr->id);

                if (meta != NULL) {
                    SWSS_LOG_INFO("Type %s attrib id %s",
                        sai_serialize_object_type(SAI_OBJECT_TYPE_ACL_ENTRY).c_str(),
                        meta->attridname);
                }

                status = tunterm_acl_rule_field_update((sai_acl_entry_attr_t) attr->id, &attr->value, &tunterm_rule);

                if (status != SAI_STATUS_SUCCESS) {
                    SWSS_LOG_ERROR("Failed to fill tunterm acl rule, status: %d", status);
                    return SAI_STATUS_FAILURE;
                }
            }
            tunterm_acl_rules.push_back(tunterm_rule);
            ace.num_rules = 1;
            tunterm_rule_index++;

            SWSS_LOG_INFO("Tunterm ACE recorded: base_index=%u, num_rules=%u",
                         ace.vpp_rule_base_index, ace.num_rules);
        } else {
            // Process regular ACL rule(s)
            vpp_acl_rule_t rule = {};
            uint8_t port_proto = 0;  // Track if port-related fields were set

            // Record the base index for this ACE
            ace.vpp_rule_base_index = acl_rule_index;
            uint32_t rules_added = 0;

            for (uint32_t i = 0; i < p_ace->attrs_count && status == SAI_STATUS_SUCCESS; i++) {
                attr = &p_ace->attrs[i];
                auto meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_ACL_ENTRY, attr->id);

                if (meta != NULL) {
                    SWSS_LOG_INFO("Type %s attrib id %s",
                        sai_serialize_object_type(SAI_OBJECT_TYPE_ACL_ENTRY).c_str(),
                        meta->attridname);
                }

                if (attr->id == SAI_ACL_ENTRY_ATTR_FIELD_ACL_RANGE_TYPE) {
                    for (uint32_t jdx = 0; jdx < p_ace->range_count; jdx++) {
                        status = acl_rule_port_range_vpp_acl_set(p_ace->range_type[jdx],
                                                        &p_ace->range_limit[jdx], &rule);
                        if (status != SAI_STATUS_SUCCESS) {
                            SWSS_LOG_ERROR("Failed to fill acl rule range, status: %d", status);
                            return SAI_STATUS_FAILURE;
                        }
                        // Mark that port range was set
                        port_proto = 1;
                    }
                } else {
                    status = acl_rule_field_update((sai_acl_entry_attr_t) attr->id, &attr->value, &rule);

                    if (status != SAI_STATUS_SUCCESS) {
                        SWSS_LOG_ERROR("Failed to fill acl rule, status: %d", status);
                        return SAI_STATUS_FAILURE;
                    }

                    // Check if port fields were set (indicates port-based rule)
                    if ((attr->id == SAI_ACL_ENTRY_ATTR_FIELD_L4_SRC_PORT ||
                         attr->id == SAI_ACL_ENTRY_ATTR_FIELD_L4_DST_PORT) &&
                        attr->value.aclfield.enable) {
                        port_proto = 1;
                    }
                }

                if (rule.srcport_or_icmptype_first != 0 || rule.dstport_or_icmpcode_first != 0) {
                    SWSS_LOG_DEBUG(
                            "Attribute %d ranges: "
                            "srcport_or_icmptype = %u - %u, "
                            "dstport_or_icmpcode = %u - %u",
                            attr->id,
                            rule.srcport_or_icmptype_first, rule.srcport_or_icmptype_last,
                            rule.dstport_or_icmpcode_first, rule.dstport_or_icmpcode_last);
                }
            }

            // If port/port_range is set but protocol is not set, create 2 rules: UDP and TCP
            if (port_proto && rule.proto == 0) {
                // Create UDP rule
                vpp_acl_rule_t udp_rule = rule;
                udp_rule.proto = IPPROTO_UDP;
                acl_rules.push_back(udp_rule);
                rules_added++;
                SWSS_LOG_INFO("Added UDP rule for port-based ACL entry");

                // Create TCP rule
                vpp_acl_rule_t tcp_rule = rule;
                tcp_rule.proto = IPPROTO_TCP;
                acl_rules.push_back(tcp_rule);
                rules_added++;
                SWSS_LOG_INFO("Added TCP rule for port-based ACL entry");
            } else {
                // Add the single rule
                acl_rules.push_back(rule);
                rules_added++;
            }

            ace.num_rules = rules_added;
            acl_rule_index += rules_added;

            SWSS_LOG_INFO("Regular ACE recorded: base_index=%u, num_rules=%u",
                         ace.vpp_rule_base_index, ace.num_rules);
        }
    }

    SWSS_LOG_INFO("fill_acl_rules complete: total %u acl_rules and %u tunterm_rules",
                 (uint32_t)acl_rules.size(), (uint32_t)tunterm_acl_rules.size());

    return SAI_STATUS_SUCCESS;
}

void SwitchVpp::cleanup_acl_tbl_config(
    acl_tbl_entries_t *&aces,
    std::list<ordered_ace_list_t> &ordered_aces,
    vpp_acl_t *&acl,
    vpp_tunterm_acl_t *&tunterm_acl)
{
    SWSS_LOG_ENTER();

    if(aces != NULL) {
        free(aces);
        aces = NULL;
    }
    if(acl != NULL) {
        free(acl);
        acl = NULL;
    }
    if(tunterm_acl != NULL) {
        free(tunterm_acl);
        tunterm_acl = NULL;
    }
    ordered_aces.clear();
}

sai_status_t SwitchVpp::acl_add_replace(
    vpp_acl_t *&acl,
    sai_object_id_t tbl_oid,
    acl_tbl_entries_t *aces,
    std::list<ordered_ace_list_t> &ordered_aces)
{
    SWSS_LOG_ENTER();

    sai_status_t        status = SAI_STATUS_SUCCESS;
    bool                acl_replace;
    uint32_t            acl_swindex;
    acl_tbl_entries_t  *p_ace = NULL;
    auto                tbl_sid = sai_serialize_object_id(tbl_oid);
    auto                vpp_idx_it = m_acl_swindex_map.find(tbl_oid);
    if (vpp_idx_it == m_acl_swindex_map.end()) {
        acl_swindex = 0;
        acl_replace = false;
    } else if (acl == NULL) {
        status = emptyAclCreate(tbl_oid);
        return status;
    } else {
        acl_swindex = vpp_idx_it->second;
        acl_replace = true;
    }

    status = vpp_acl_add_replace(acl, &acl_swindex, acl_replace);
    if (status == SAI_STATUS_SUCCESS) {
        m_acl_swindex_map[tbl_oid] = acl_swindex;
        for (auto ace: ordered_aces) {
            p_ace = &aces[ace.index];
            const sai_attribute_t *attr;
            sai_object_id_t ace_cntr_oid;
            for (uint32_t i = 0; i < p_ace->attrs_count; i++) {
                attr = &p_ace->attrs[i];
                if (attr->id == SAI_ACL_ENTRY_ATTR_ACTION_COUNTER) {
                    ace_cntr_oid = attr->value.aclaction.parameter.oid;
                    auto ace_it = m_ace_cntr_info_map.find(ace_cntr_oid);
                    if (ace_it != m_ace_cntr_info_map.end()) {
                        m_ace_cntr_info_map.erase(ace_it);
                    }
                    // For stats we need to find vpp rule index from acl_entry_counter (ace_counter)
                    m_ace_cntr_info_map[ace_cntr_oid] = { tbl_oid, ace.ace_oid, acl_swindex, ace.vpp_rule_base_index, ace.num_rules};
                }
            }
        }
    } else {
        SWSS_LOG_ERROR("Vpp acl add replace failed, status: %d", status);
        return SAI_STATUS_FAILURE;
    }

    SWSS_LOG_INFO("ACL table %s %s ", tbl_sid.c_str(),
            acl_replace ? "replaced" : "added");

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::tunterm_acl_bindunbind(sai_object_id_t tbl_oid, bool is_add, std::string hwif_name)
{
    SWSS_LOG_ENTER();

    sai_status_t status = SAI_STATUS_SUCCESS;
    auto tunterm_idx_it = m_tunterm_acl_swindex_map.find(tbl_oid);
    if (tunterm_idx_it != m_tunterm_acl_swindex_map.end()) {
        auto tunterm_idx = tunterm_idx_it->second;
        status = vpp_tunterm_acl_interface_add_del(tunterm_idx, is_add, hwif_name.c_str());
    }
    if(status != SAI_STATUS_SUCCESS) {
        SWSS_LOG_ERROR("Tunterm acl bind/unbind failed to hwif %s, status: %d",
                        hwif_name.c_str(), status);
    }
    return status;
}

sai_status_t SwitchVpp::tunterm_acl_add_replace(vpp_tunterm_acl_t *acl, sai_object_id_t tbl_oid)
{
    SWSS_LOG_ENTER();

    sai_status_t status = SAI_STATUS_SUCCESS;
    uint32_t     tunterm_acl_swindex = 0;
    bool         do_port_bind = false;
    auto         tbl_sid = sai_serialize_object_id(tbl_oid);

    auto tunterm_idx_it = m_tunterm_acl_swindex_map.find(tbl_oid);
    if ((tunterm_idx_it == m_tunterm_acl_swindex_map.end()) && (acl != NULL)) {
        // ADD new tunterm acl
        tunterm_acl_swindex = ~tunterm_acl_swindex;
        do_port_bind = true;
    } else if (tunterm_idx_it != m_tunterm_acl_swindex_map.end() && (acl == NULL)) {
        // REPLACE with empty ACL (delete tunterm acl)
        return tunterm_acl_delete(tbl_oid, false);
    } else if (tunterm_idx_it != m_tunterm_acl_swindex_map.end()) {
        // REPLACE with incoming tunterm acl
        tunterm_acl_swindex = tunterm_idx_it->second;
    } else {
        // NO-OP, tunterm acl not configured and no incoming tunterm acl.
        return status;
    }

    std::list<std::string> hwif_names;
    SWSS_LOG_INFO("Adding tunterm acl rules, tunterm_acl index %u", tunterm_acl_swindex);

    status = vpp_tunterm_acl_add_replace(&tunterm_acl_swindex, acl->count, acl);

    if (status == SAI_STATUS_SUCCESS) {
        m_tunterm_acl_swindex_map[tbl_oid] = tunterm_acl_swindex;
    } else {
        return SAI_STATUS_FAILURE;
    }

    /*
     *  Bind tunterm acl to ports which ACL table is already bound when first
     *  tunterm ACL rule is received.
     */
    if (do_port_bind) {
        auto it_hw_ports = m_acl_tbl_hw_ports_map.find(tbl_oid);
        if (it_hw_ports == m_acl_tbl_hw_ports_map.end()) {
            auto sid = sai_serialize_object_id(tbl_oid);
            SWSS_LOG_INFO("Tunterm acl - no ports bound for table id %s", sid.c_str());
        } else {
            hwif_names = it_hw_ports->second;
            for (auto hwif_name: hwif_names) {
                SWSS_LOG_INFO("Tunterm acl - binding to hwif %s", hwif_name.c_str());
                status = tunterm_acl_bindunbind(tbl_oid, true, hwif_name);
                if(status != SAI_STATUS_SUCCESS) {
                    return status;
                }
            }
        }
    }

    SWSS_LOG_INFO("Tunterm ACL table %s %s status %d", tbl_sid.c_str(),
            do_port_bind ? "add" : "replace", status);

    return status;
}

sai_status_t SwitchVpp::tbl_hw_ports_map_delete(sai_object_id_t tbl_oid)
{
    SWSS_LOG_ENTER();

    auto hw_ports_it = m_acl_tbl_hw_ports_map.find(tbl_oid);
    if (hw_ports_it == m_acl_tbl_hw_ports_map.end()) {
        return SAI_STATUS_SUCCESS;
    }
    m_acl_tbl_hw_ports_map.erase(hw_ports_it);
    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::tunterm_acl_delete(sai_object_id_t tbl_oid, bool table_delete)
{
    SWSS_LOG_ENTER();

    sai_status_t status = SAI_STATUS_SUCCESS;
    auto         tunterm_idx_it = m_tunterm_acl_swindex_map.find(tbl_oid);

    if (tunterm_idx_it == m_tunterm_acl_swindex_map.end()) {
        SWSS_LOG_WARN("No tunterm ACL configured for table %s",
                        sai_serialize_object_id(tbl_oid).c_str());
        return status;
    }

    /*
     *  In the case where tunterm ACL is deleted by ACL update with empty ACL,
     *  need to unbind the ports here before tunterm ACL can be deleted. In the
     *  regular tunterm ACL delete case, the ports are unbound via SAI calls.
     */
    if(!table_delete) {
        for(auto hwif_name:  m_acl_tbl_hw_ports_map[tbl_oid]) {
            status = tunterm_acl_bindunbind(tbl_oid, false, hwif_name);
            if(status != SAI_STATUS_SUCCESS) {
                return status;
            }
        }
    }

    uint32_t tunterm_acl_swindex = tunterm_idx_it->second;
    status = vpp_tunterm_acl_del(tunterm_acl_swindex);
    if (status == SAI_STATUS_SUCCESS) {
        m_tunterm_acl_swindex_map.erase(tunterm_idx_it);
        if(table_delete) {
            tbl_hw_ports_map_delete(tbl_oid);
        }
    }

    SWSS_LOG_INFO("Tunterm acl table %s, remove tunterm_acl index %u, "
                    "status %d", sai_serialize_object_id(tbl_oid).c_str(),
                    tunterm_acl_swindex, status);

    return status;
}

sai_status_t SwitchVpp::AclTblConfig(
    _In_ sai_object_id_t tbl_oid)
{
    SWSS_LOG_ENTER();

    sai_status_t                        status = SAI_STATUS_SUCCESS;
    size_t                              n_total_entries = 0;
    acl_tbl_entries_t                  *aces = NULL;
    vpp_acl_t                          *acl = NULL;
    vpp_tunterm_acl_t                  *tunterm_acl = NULL;
    char                                aclname[64];
    char                                tunterm_aclname[64];
    std::list<ordered_ace_list_t>       ordered_aces = {};
    std::list<vpp_acl_rule_t>          acl_rules;
    std::list<vpp_tunterm_acl_rule_t>  tunterm_acl_rules;

    #define CHECK_STATUS_ACLTBLCONFIG(status) {                           \
        sai_status_t _status = (status);                                  \
        if (_status != SAI_STATUS_SUCCESS) {                              \
            cleanup_acl_tbl_config(aces, ordered_aces, acl, tunterm_acl); \
            return _status;                                               \
        }                                                                 \
    }

    CHECK_STATUS_ACLTBLCONFIG(get_sorted_aces(tbl_oid, n_total_entries, aces,
                                              ordered_aces));

    SWSS_LOG_INFO("Total ACL entries: %ld", n_total_entries);

    // Fill ACL rules - this returns converted rule lists
    CHECK_STATUS_ACLTBLCONFIG(fill_acl_rules(aces, ordered_aces, acl_rules, tunterm_acl_rules));

    SWSS_LOG_INFO("Generated %ld regular ACL rules and %ld tunterm ACL rules",
                    acl_rules.size(), tunterm_acl_rules.size());

    // Create and populate regular ACL if we have rules
    if (!acl_rules.empty()) {
        auto tbl_sid = sai_serialize_object_id(tbl_oid);

        acl = (vpp_acl_t *) calloc(1, sizeof(vpp_acl_t) + (acl_rules.size() * sizeof(vpp_acl_rule_t)));
        if (!acl) {
            SWSS_LOG_ERROR("Failed to allocate memory for acl.");
            cleanup_acl_tbl_config(aces, ordered_aces, acl, tunterm_acl);
            return SAI_STATUS_FAILURE;
        }

        snprintf(aclname, sizeof(aclname), "sonic_acl_%s", tbl_sid.c_str());
        acl->acl_name = aclname;
        acl->count = (uint32_t) acl_rules.size();

        // Copy rules into ACL structure
        vpp_acl_rule_t *rule_ptr = &acl->rules[0];
        for (const auto &rule : acl_rules) {
            *rule_ptr++ = rule;
        }

        SWSS_LOG_INFO("Allocated ACL with %u rules", acl->count);
    }

    // Create and populate tunnel termination ACL if we have rules
    if (!tunterm_acl_rules.empty()) {
        auto tbl_sid = sai_serialize_object_id(tbl_oid);

        tunterm_acl = (vpp_tunterm_acl_t *) calloc(1, sizeof(vpp_tunterm_acl_t) + (tunterm_acl_rules.size() * sizeof(vpp_tunterm_acl_rule_t)));
        if (!tunterm_acl) {
            SWSS_LOG_ERROR("Failed to allocate memory for tunterm acl.");
            cleanup_acl_tbl_config(aces, ordered_aces, acl, tunterm_acl);
            return SAI_STATUS_FAILURE;
        }

        snprintf(tunterm_aclname, sizeof(tunterm_aclname), "tunterm_sonic_acl_%s", tbl_sid.c_str());
        tunterm_acl->acl_name = tunterm_aclname;
        tunterm_acl->count = (uint32_t) tunterm_acl_rules.size();

        // Copy rules into tunterm ACL structure
        vpp_tunterm_acl_rule_t *tunterm_rule_ptr = &tunterm_acl->rules[0];
        for (const auto &rule : tunterm_acl_rules) {
            *tunterm_rule_ptr++ = rule;
        }

        SWSS_LOG_INFO("Allocated tunterm ACL with %u rules", tunterm_acl->count);
    }

    // Apply the ACL configurations
    if (acl != NULL) {
        status = acl_add_replace(acl, tbl_oid, aces, ordered_aces);
    }

    if (status == SAI_STATUS_SUCCESS && tunterm_acl != NULL) {
        status = tunterm_acl_add_replace(tunterm_acl, tbl_oid);
    } else if (status != SAI_STATUS_SUCCESS) {
        SWSS_LOG_ERROR("ACL plugin operation failed, skipping tunterm "
                       "configuration. status %d", status);
    }

    cleanup_acl_tbl_config(aces, ordered_aces, acl, tunterm_acl);
    return status;
}

sai_status_t SwitchVpp::aclGetVppIndices(
    _In_ sai_object_id_t ace_cntr_oid,
    _Out_ uint32_t *acl_index,
    _Out_ uint32_t *vpp_rule_base_index,
    _Out_ uint32_t *num_rules)
{
    SWSS_LOG_ENTER();

    auto vpp_ace_it = m_ace_cntr_info_map.find(ace_cntr_oid);
    if (vpp_ace_it == m_ace_cntr_info_map.end()) {
        SWSS_LOG_WARN("VS ace entry %s not found in vpp_ace_cntr_info_map",
                      sai_serialize_object_id(ace_cntr_oid).c_str());
        return SAI_STATUS_FAILURE;
    }
    auto & ace_info = vpp_ace_it->second;

    *acl_index = ace_info.acl_index;
    *vpp_rule_base_index = ace_info.vpp_rule_base_index;
    *num_rules  = ace_info.num_rules;

    SWSS_LOG_INFO("VS acl index %u vpp_rule_base_index %u num vpp rules %u for acl_counter %s acl_table %s",
                    *acl_index, *vpp_rule_base_index, *num_rules,
                    sai_serialize_object_id(ace_cntr_oid).c_str(),
                    sai_serialize_object_id(ace_info.tbl_oid).c_str());

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::aclTableCreate(
        _In_ sai_object_id_t object_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    auto sid = sai_serialize_object_id(object_id);

    CHECK_STATUS(create_internal(SAI_OBJECT_TYPE_ACL_TABLE, sid, switch_id, attr_count, attr_list));

    SWSS_LOG_NOTICE("ACL table %s created", sid.c_str());

    return emptyAclCreate(object_id);
}

sai_status_t SwitchVpp::aclTableRemove(
    _In_ const std::string &serializedObjectId)
{
    SWSS_LOG_ENTER();

    sai_object_id_t tbl_oid;

    sai_deserialize_object_id(serializedObjectId, tbl_oid);

    CHECK_STATUS(remove_internal(SAI_OBJECT_TYPE_ACL_TABLE, serializedObjectId));

    return AclTblRemove(tbl_oid);
}

sai_status_t SwitchVpp::emptyAclCreate(
    _In_ sai_object_id_t tbl_oid)
{
    SWSS_LOG_ENTER();

    uint32_t        acl_swindex = 0;
    bool            acl_replace = false;
    auto            vpp_idx_it = m_acl_swindex_map.find(tbl_oid);
    if (vpp_idx_it != m_acl_swindex_map.end()) {
        acl_swindex = vpp_idx_it->second;
        acl_replace = true;
    }

    vpp_acl_t *acl;

    // Create an ACL with 1 rule matching dest IP 0.0.0.0/32 (will never match real traffic) but vpp doesn't allow 0 rule table
    acl = (vpp_acl_t *) calloc(1, sizeof(vpp_acl_t) + sizeof(vpp_acl_rule_t));
    if (!acl) {
        return SAI_STATUS_FAILURE;
    }
    acl->count = 1;
    char aclname[64];

    auto sid = sai_serialize_object_id(tbl_oid);

    snprintf(aclname, sizeof(aclname), "sonic_acl_%s", sid.c_str());
    acl->acl_name = aclname;

    // Set up rule to match dest IP 0.0.0.0/32
    vpp_acl_rule_t *rule = &acl->rules[0];
    rule->dst_prefix.sa_family = AF_INET;
    rule->dst_prefix.addr.ip4.sin_addr.s_addr = 0;  // 0.0.0.0
    rule->dst_prefix_mask.addr.ip4.sin_addr.s_addr = 0xFFFFFFFF;  // 255.255.255.255
    rule->action = VPP_ACL_ACTION_API_PERMIT;

    sai_status_t status;

    status = vpp_acl_add_replace(acl, &acl_swindex, acl_replace);
    if (status == SAI_STATUS_SUCCESS && !acl_replace) {
        m_acl_swindex_map[tbl_oid] = acl_swindex;
    }

    free(acl);
    SWSS_LOG_INFO("Placeholder ACL for table %s created, status %d swindex %u",
                    sid.c_str(), status, acl_swindex);
    return status;
}

sai_status_t SwitchVpp::aclDefaultCreate()
{
    SWSS_LOG_ENTER();

    // If already created, return success
    if (m_acl_default_created) {
        return SAI_STATUS_SUCCESS;
    }

    sai_attribute_t attr[2];

    attr[0].id = SAI_ACL_ENTRY_ATTR_FIELD_ACL_IP_TYPE;
    attr[0].value.aclfield.enable = true;
    attr[0].value.aclfield.data.s32 = SAI_ACL_IP_TYPE_IPV4ANY;

    attr[1].id = SAI_ACL_ENTRY_ATTR_FIELD_ACL_IP_TYPE;
    attr[1].value.aclfield.enable = true;
    attr[1].value.aclfield.data.s32 = SAI_ACL_IP_TYPE_IPV6ANY;

    vpp_acl_t *acl;

    acl = (vpp_acl_t *) calloc(1, sizeof(vpp_acl_t) + (DEFAULT_PERMIT_RULES * sizeof(vpp_acl_rule_t)));
    if (!acl) {
        return SAI_STATUS_FAILURE;
    }
    acl->count = DEFAULT_PERMIT_RULES;

    char aclname[64];
    snprintf(aclname, sizeof(aclname), "sonic_acl_default_permit");
    acl->acl_name = aclname;

    vpp_acl_rule_t *rule = &acl->rules[0];

    acl_rule_field_update((sai_acl_entry_attr_t) attr[0].id, &attr[0].value, rule);
    rule->action = VPP_ACL_ACTION_API_PERMIT;

    rule = &acl->rules[1];

    acl_rule_field_update((sai_acl_entry_attr_t) attr[1].id, &attr[1].value, rule);
    rule->action = VPP_ACL_ACTION_API_PERMIT;

    sai_status_t status;

    status = vpp_acl_add_replace(acl, &m_acl_default_swindex, false);
    if (status == SAI_STATUS_SUCCESS) {
        m_acl_default_created = true;
        SWSS_LOG_NOTICE("Shared default permit ACL created with swindex %u", m_acl_default_swindex);
    } else {
        SWSS_LOG_ERROR("Failed to create shared default permit ACL");
    }

    free(acl);
    return status;
}

sai_status_t SwitchVpp::AclTblRemove(
    _In_ sai_object_id_t tbl_oid)
{
    SWSS_LOG_ENTER();

    sai_status_t status;

    auto it = m_acl_tbl_rules_map.find(tbl_oid);

    if (it != m_acl_tbl_rules_map.end()) {
        std::list<sai_object_id_t>& member_list = it->second;

        if (member_list.size()) {
            member_list.clear();
        }
        m_acl_tbl_rules_map.erase(it);
    }

    status = tunterm_acl_delete(tbl_oid, true);

    auto vpp_idx_it = m_acl_swindex_map.find(tbl_oid);
    if (vpp_idx_it == m_acl_swindex_map.end()) {
        SWSS_LOG_WARN("No ACL configured for table %s", sai_serialize_object_id(tbl_oid).c_str());
        return SAI_STATUS_FAILURE;
    }
    uint32_t acl_swindex = vpp_idx_it->second;

    status = vpp_acl_del(acl_swindex);

    if (status == SAI_STATUS_SUCCESS) {
        m_acl_swindex_map.erase(vpp_idx_it);
    }
    SWSS_LOG_NOTICE("ACL table %s remove swindex %u status %d",
                    sai_serialize_object_id(tbl_oid).c_str(), acl_swindex, status);

    return status;
}

sai_status_t SwitchVpp::addRemoveAclEntrytoMap(
    _In_ sai_object_id_t entry_id,
    _In_ sai_object_id_t tbl_oid,
    _In_ bool is_add)
{
    SWSS_LOG_ENTER();

    auto it = m_acl_tbl_rules_map.find(tbl_oid);

    if (it == m_acl_tbl_rules_map.end()) {

        if (!is_add) {
            auto sid = sai_serialize_object_id(entry_id);
            SWSS_LOG_ERROR("ACL entry with id %s not found in tbl %s", sid.c_str(),
                           sai_serialize_object_id(tbl_oid).c_str());
            return SAI_STATUS_FAILURE;
        }

        std::list<sai_object_id_t> member_list;

        member_list = { entry_id };
        m_acl_tbl_rules_map[tbl_oid] = member_list;

    } else {
        std::list<sai_object_id_t>& member_list = it->second;

        if (!is_add) {
            member_list.remove(entry_id);
            return SAI_STATUS_SUCCESS;
        }
        member_list.push_back(entry_id);
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::AclAddRemoveCheck(
    _In_ sai_object_id_t tbl_oid)
{
    SWSS_LOG_ENTER();

    sai_status_t status = SAI_STATUS_SUCCESS;
    auto it = m_acl_tbl_rules_map.find(tbl_oid);

    if (it == m_acl_tbl_rules_map.end()) {
        return SAI_STATUS_SUCCESS;
    }

    status = AclTblConfig(tbl_oid);
    return status;
}

sai_status_t SwitchVpp::createAclEntry(
        _In_ sai_object_id_t object_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    auto sid = sai_serialize_object_id(object_id);

    CHECK_STATUS(create_internal(SAI_OBJECT_TYPE_ACL_ENTRY, sid, switch_id, attr_count, attr_list));

    sai_object_id_t tbl_oid;

    if (getAclTableId(object_id, &tbl_oid) != SAI_STATUS_SUCCESS) {
        return SAI_STATUS_FAILURE;
    }
    sai_status_t status;

    status = addRemoveAclEntrytoMap(object_id, tbl_oid, true);
    if (status == SAI_STATUS_SUCCESS) {
        status = AclAddRemoveCheck(tbl_oid);
    }
    return status;
}

sai_status_t SwitchVpp::removeAclEntry(
        _In_ const std::string &serializedObjectId)
{
    SWSS_LOG_ENTER();

    sai_object_id_t entry_oid;

    sai_deserialize_object_id(serializedObjectId, entry_oid);

    sai_object_id_t tbl_oid;

    if (getAclTableId(entry_oid, &tbl_oid) != SAI_STATUS_SUCCESS) {
        return SAI_STATUS_FAILURE;
    }

    sai_status_t status;

    status = addRemoveAclEntrytoMap(entry_oid, tbl_oid, false);
    if (status == SAI_STATUS_SUCCESS) {
        status = AclAddRemoveCheck(tbl_oid);
    }
    remove_internal(SAI_OBJECT_TYPE_ACL_ENTRY, serializedObjectId);

    SWSS_LOG_NOTICE("ACL entry %s in table %s remove status %d",
        sai_serialize_object_id(entry_oid).c_str(),
        sai_serialize_object_id(tbl_oid).c_str(),
        status);

    return status;
}

sai_status_t SwitchVpp::getAclTableGroupId(
    _In_ sai_object_id_t member_oid,
    _Out_ sai_object_id_t *tbl_grp_oid)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;

    attr.id = SAI_ACL_TABLE_GROUP_MEMBER_ATTR_ACL_TABLE_GROUP_ID;
    if (get(SAI_OBJECT_TYPE_ACL_TABLE_GROUP_MEMBER, member_oid, 1, &attr) != SAI_STATUS_SUCCESS) {
        auto sid = sai_serialize_object_id(member_oid);

        SWSS_LOG_INFO("ACL table group oid for acl grp member id %s not found", sid.c_str());
        return SAI_STATUS_FAILURE;
    }

    *tbl_grp_oid = attr.value.oid;

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::addRemoveAclGrpMbr(
    _In_ sai_object_id_t member_oid,
    _In_ sai_object_id_t tbl_grp_oid,
    _In_ bool is_add)
{
    SWSS_LOG_ENTER();

    // Get the ACL stage (ingress/egress) from the table group
    sai_attribute_t attr;
    attr.id = SAI_ACL_TABLE_GROUP_ATTR_ACL_STAGE;
    if (get(SAI_OBJECT_TYPE_ACL_TABLE_GROUP, tbl_grp_oid, 1, &attr) != SAI_STATUS_SUCCESS) {
        SWSS_LOG_INFO("ACL table group %s direction not found",
                      sai_serialize_object_id(tbl_grp_oid).c_str());
        return SAI_STATUS_FAILURE;
    }

    bool is_input;
    switch (attr.value.s32) {
    case SAI_ACL_STAGE_INGRESS:
        is_input = true;
        break;
    case SAI_ACL_STAGE_EGRESS:
        is_input = false;
        break;
    default:
        SWSS_LOG_INFO("ACL table group %s unsupported direction %d",
                      sai_serialize_object_id(tbl_grp_oid).c_str(), attr.value.s32);
        return SAI_STATUS_FAILURE;
    }

    // Step 1: Unbind existing ACLs from all ports
    // Make a copy of ports list since aclBindUnbindPort modifies m_acl_tbl_grp_ports_map
    std::list<sai_object_id_t> ports_copy;
    auto ports_it = m_acl_tbl_grp_ports_map.find(tbl_grp_oid);
    if (ports_it != m_acl_tbl_grp_ports_map.end()) {
        ports_copy = ports_it->second;
        for (auto port_oid : ports_copy) {
            sai_status_t status = aclBindUnbindPort(port_oid, tbl_grp_oid, is_input, false);
            if (status != SAI_STATUS_SUCCESS) {
                SWSS_LOG_ERROR("Failed to unbind ACL group %s from port %s",
                               sai_serialize_object_id(tbl_grp_oid).c_str(),
                               sai_serialize_object_id(port_oid).c_str());
                return status;
            }
        }
    }

    // Step 2: Update m_acl_tbl_grp_mbr_map with add or remove group member
    auto mbr_it = m_acl_tbl_grp_mbr_map.find(tbl_grp_oid);

    if (mbr_it == m_acl_tbl_grp_mbr_map.end()) {
        if (!is_add) {
            SWSS_LOG_ERROR("ACL group member %s not found in tbl group %s",
                           sai_serialize_object_id(member_oid).c_str(),
                           sai_serialize_object_id(tbl_grp_oid).c_str());
            return SAI_STATUS_FAILURE;
        }

        std::list<sai_object_id_t> member_list = { member_oid };
        m_acl_tbl_grp_mbr_map[tbl_grp_oid] = member_list;
    } else {
        std::list<sai_object_id_t>& member_list = mbr_it->second;

        if (is_add) {
            member_list.push_back(member_oid);
        } else {
            member_list.remove(member_oid);
        }
    }

    // Step 3: Rebind ACLs to all ports with updated member list
    for (auto port_oid : ports_copy) {
        sai_status_t status = aclBindUnbindPort(port_oid, tbl_grp_oid, is_input, true);
        if (status != SAI_STATUS_SUCCESS) {
            SWSS_LOG_ERROR("Failed to bind ACL group %s to port %s",
                           sai_serialize_object_id(tbl_grp_oid).c_str(),
                           sai_serialize_object_id(port_oid).c_str());
            return status;
        }
    }

    SWSS_LOG_NOTICE("ACL group member %s %s table group %s",
                    sai_serialize_object_id(member_oid).c_str(),
                    is_add ? "added to" : "removed from",
                    sai_serialize_object_id(tbl_grp_oid).c_str());
    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::createAclGrpMbr(
        _In_ sai_object_id_t object_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    auto sid = sai_serialize_object_id(object_id);

    CHECK_STATUS(create_internal(SAI_OBJECT_TYPE_ACL_TABLE_GROUP_MEMBER,
                                 sid, switch_id, attr_count, attr_list));

    sai_object_id_t tbl_grp_oid;

    if (getAclTableGroupId(object_id, &tbl_grp_oid) != SAI_STATUS_SUCCESS) {
        return SAI_STATUS_FAILURE;
    }
    sai_status_t status;

    status = addRemoveAclGrpMbr(object_id, tbl_grp_oid, true);

    return status;
}

sai_status_t SwitchVpp::removeAclGrpMbr(
        _In_ const std::string &serializedObjectId)
{
    SWSS_LOG_ENTER();

    sai_object_id_t member_oid;

    sai_deserialize_object_id(serializedObjectId, member_oid);

    sai_object_id_t tbl_grp_oid;

    if (getAclTableGroupId(member_oid, &tbl_grp_oid) != SAI_STATUS_SUCCESS) {
        return SAI_STATUS_FAILURE;
    }

    sai_status_t status;

    status = addRemoveAclGrpMbr(member_oid, tbl_grp_oid, false);

    SWSS_LOG_NOTICE("Remove Acl grp member %s status %d",
                    serializedObjectId.c_str(), status);

    remove_internal(SAI_OBJECT_TYPE_ACL_TABLE_GROUP_MEMBER, serializedObjectId);

    return status;
}

sai_status_t SwitchVpp::setAclGrpMbr(
        _In_ sai_object_id_t member_oid,
        _In_ const sai_attribute_t* attr)
{
    SWSS_LOG_ENTER();

    sai_status_t status = SAI_STATUS_SUCCESS;

    sai_object_id_t tbl_grp_oid = SAI_NULL_OBJECT_ID;

    getAclTableGroupId(member_oid, &tbl_grp_oid);

    if (attr->id == SAI_ACL_TABLE_GROUP_MEMBER_ATTR_ACL_TABLE_GROUP_ID) {
        if (tbl_grp_oid == SAI_NULL_OBJECT_ID) {
            status = addRemoveAclGrpMbr(member_oid, tbl_grp_oid, true);
        } else {
            status = addRemoveAclGrpMbr(member_oid, tbl_grp_oid, false);
            if (status == SAI_STATUS_SUCCESS) {
                status = addRemoveAclGrpMbr(member_oid, attr->value.oid, true);
            }
        }
    }
    auto sid = sai_serialize_object_id(member_oid);

    SWSS_LOG_NOTICE("ACL grp member %s set attr %d", sid.c_str(), attr->id);

    set_internal(SAI_OBJECT_TYPE_ACL_TABLE_GROUP_MEMBER, sid, attr);

    return status;
}

sai_status_t SwitchVpp::removeAclGrp(
    _In_ const std::string &serializedObjectId)
{
    SWSS_LOG_ENTER();

    sai_object_id_t tbl_grp_oid;

    sai_deserialize_object_id(serializedObjectId, tbl_grp_oid);

    auto it = m_acl_tbl_grp_mbr_map.find(tbl_grp_oid);

    if (it != m_acl_tbl_grp_mbr_map.end()) {

        std::list<sai_object_id_t>& member_list = it->second;
        sai_status_t status;

        for (sai_object_id_t member_oid: member_list) {

            status = addRemoveAclGrpMbr(member_oid, tbl_grp_oid, false);
            if (status != SAI_STATUS_SUCCESS) {
                SWSS_LOG_WARN("Failed to delete ACL tbl grp member %s from group %s",
                              sai_serialize_object_id(member_oid).c_str(),
                              serializedObjectId.c_str());
            }
        }
        m_acl_tbl_grp_mbr_map.erase(it);
    }
    SWSS_LOG_NOTICE("Remove ACL group %s", serializedObjectId.c_str());

    return remove_internal(SAI_OBJECT_TYPE_ACL_TABLE_GROUP, serializedObjectId);
}

sai_status_t SwitchVpp::addRemovePortTblGrp(
    _In_ sai_object_id_t port_oid,
    _In_ sai_object_id_t tbl_grp_oid,
    _In_ bool is_add)
{
    SWSS_LOG_ENTER();

    auto it = m_acl_tbl_grp_ports_map.find(tbl_grp_oid);

    if (it == m_acl_tbl_grp_ports_map.end()) {
        if (!is_add) {
            SWSS_LOG_INFO("port id %s delete failed, no table group %s",
                            sai_serialize_object_id(port_oid).c_str(),
                            sai_serialize_object_id(tbl_grp_oid).c_str());
            return SAI_STATUS_FAILURE;
        }
        std::list<sai_object_id_t> ports_list = { port_oid };
        m_acl_tbl_grp_ports_map[tbl_grp_oid] = ports_list;
    } else {
        std::list<sai_object_id_t>& ports_list = it->second;
        if (is_add) {
            ports_list.push_back(port_oid);
        } else {
            ports_list.remove(port_oid);
        }
    }
    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::aclBindUnbindPort(
        _In_ sai_object_id_t port_oid,
        _In_ sai_object_id_t tbl_grp_oid,
        _In_ bool is_input,
        _In_ bool is_bind)
{
    SWSS_LOG_ENTER();

    addRemovePortTblGrp(port_oid, tbl_grp_oid, is_bind);

    auto it = m_acl_tbl_grp_mbr_map.find(tbl_grp_oid);

    if (it == m_acl_tbl_grp_mbr_map.end()) {
        auto sid = sai_serialize_object_id(tbl_grp_oid);
        SWSS_LOG_INFO("ACL tbl group with id %s not found", sid.c_str());
        /*
         * The tbl group is not created until a group member is added. The bind port
         * will be called later when a group member is added.
         */
        return SAI_STATUS_SUCCESS;
    }

    std::list<sai_object_id_t>& member_list = it->second;

    // If member_list is empty, do nothing
    if (member_list.empty()) {
        SWSS_LOG_INFO("ACL tbl group %s has no members", sai_serialize_object_id(tbl_grp_oid).c_str());
        return SAI_STATUS_SUCCESS;
    }

    std::string hwif_name;

    if (!vpp_get_hwif_name(port_oid, 0, hwif_name)) {
        SWSS_LOG_WARN("VS hwif name not found for port %s", sai_serialize_object_id(port_oid).c_str());
        return SAI_STATUS_FAILURE;
    }

    // Build a vector of (priority, acl_swindex) for sorting
    std::vector<std::pair<uint32_t, uint32_t>> sorted_members;

    for (auto member_oid: member_list) {
        sai_attribute_t attr;

        // Get ACL table OID
        attr.id = SAI_ACL_TABLE_GROUP_MEMBER_ATTR_ACL_TABLE_ID;
        if (get(SAI_OBJECT_TYPE_ACL_TABLE_GROUP_MEMBER, member_oid, 1, &attr) != SAI_STATUS_SUCCESS) {
            SWSS_LOG_INFO("ACL table oid for acl grp member id %s not found",
                          sai_serialize_object_id(member_oid).c_str());
            return SAI_STATUS_FAILURE;
        }
        auto tbl_oid = attr.value.oid;

        // Get VPP swindex for the ACL table
        auto vpp_idx_it = m_acl_swindex_map.find(tbl_oid);
        if (vpp_idx_it == m_acl_swindex_map.end()) {
            SWSS_LOG_INFO("VS swindex for ACL table oid %s not found",
                          sai_serialize_object_id(tbl_oid).c_str());
            return SAI_STATUS_FAILURE;
        }
        auto acl_swindex = vpp_idx_it->second;

        // Get member priority
        attr.id = SAI_ACL_TABLE_GROUP_MEMBER_ATTR_PRIORITY;
        uint32_t priority = 0;
        if (get(SAI_OBJECT_TYPE_ACL_TABLE_GROUP_MEMBER, member_oid, 1, &attr) == SAI_STATUS_SUCCESS) {
            priority = attr.value.u32;
        }

        sorted_members.push_back({priority, acl_swindex});
    }

    // Sort by priority in descending order (higher priority first)
    std::sort(sorted_members.begin(), sorted_members.end(),
              [](const std::pair<uint32_t, uint32_t>& a, const std::pair<uint32_t, uint32_t>& b) {
                  return a.first > b.first;
              });

    // Bind/unbind each ACL in sorted order
    for (const auto& member : sorted_members) {
        uint32_t acl_swindex = member.second;
        int ret;

        if (is_bind)
            ret = vpp_acl_interface_bind(hwif_name.c_str(), acl_swindex, is_input);
        else
            ret = vpp_acl_interface_unbind(hwif_name.c_str(), acl_swindex, is_input);

        if (ret != 0) {
            SWSS_LOG_ERROR("VS Acl swindex %u %s failed for port %s", acl_swindex,
                           is_bind ? "bind" : "unbind", hwif_name.c_str());
            return SAI_STATUS_FAILURE;
        }
        SWSS_LOG_NOTICE("ACL swindex %u %s to port %s (priority %u)", acl_swindex,
                        is_bind ? "bound" : "unbound", hwif_name.c_str(), member.first);
    }

    // Handle the shared default ACL
    if (is_bind) {
        // Create default ACL if not already created
        CHECK_STATUS(aclDefaultCreate());

        // Bind default ACL
        int ret = vpp_acl_interface_bind(hwif_name.c_str(), m_acl_default_swindex, is_input);
        if (ret != 0) {
            SWSS_LOG_ERROR("Default ACL swindex %u bind failed for port %s",
                           m_acl_default_swindex, hwif_name.c_str());
            return SAI_STATUS_FAILURE;
        }
        SWSS_LOG_NOTICE("Default ACL swindex %u bound to port %s",
                        m_acl_default_swindex, hwif_name.c_str());
    } else {
        // Unbind default ACL (if it was created)
        if (m_acl_default_created) {
            int ret = vpp_acl_interface_unbind(hwif_name.c_str(), m_acl_default_swindex, is_input);
            if (ret != 0) {
                SWSS_LOG_ERROR("Default ACL swindex %u unbind failed for port %s",
                               m_acl_default_swindex, hwif_name.c_str());
                return SAI_STATUS_FAILURE;
            }
            SWSS_LOG_NOTICE("Default ACL swindex %u unbound from port %s",
                            m_acl_default_swindex, hwif_name.c_str());
        }
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::aclBindUnbindPorts(
        _In_ sai_object_id_t tbl_grp_oid,
        _In_ sai_object_id_t tbl_oid,
        _In_ bool is_bind)
{
    SWSS_LOG_ENTER();

    auto it = m_acl_tbl_grp_ports_map.find(tbl_grp_oid);

    if (it == m_acl_tbl_grp_ports_map.end()) {
        auto sid = sai_serialize_object_id(tbl_grp_oid);
        SWSS_LOG_INFO("ACL tbl group with id %s not found in acl ports map", sid.c_str());
        return SAI_STATUS_SUCCESS;
    }
    sai_attribute_t attr;

    attr.id = SAI_ACL_TABLE_GROUP_ATTR_ACL_STAGE;
    if (get(SAI_OBJECT_TYPE_ACL_TABLE_GROUP, tbl_grp_oid, 1, &attr) != SAI_STATUS_SUCCESS) {
        auto sid = sai_serialize_object_id(tbl_grp_oid);

        SWSS_LOG_INFO("ACL table group %s direction not found", sid.c_str());
        return SAI_STATUS_SUCCESS;
    }

    int dir = attr.value.s32;
    bool is_input;

    switch (dir) {
    case SAI_ACL_STAGE_INGRESS:
        is_input = true;
        break;

    case SAI_ACL_STAGE_EGRESS:
        is_input = false;
        break;

    default:
    {
        auto sid = sai_serialize_object_id(tbl_grp_oid);
        SWSS_LOG_INFO("ACL table group %s direction %d", sid.c_str(), dir);
        return SAI_STATUS_SUCCESS;
    }
    }

    auto vpp_idx_it = m_acl_swindex_map.find(tbl_oid);
    if (vpp_idx_it == m_acl_swindex_map.end()) {
        auto sid = sai_serialize_object_id(tbl_oid);
        SWSS_LOG_INFO("VS swindex for ACL table oid %s not found", sid.c_str());
        return SAI_STATUS_FAILURE;
    }
    auto acl_swindex = vpp_idx_it->second;

    std::list<sai_object_id_t>& member_list = it->second;
    std::string hwif_name;
    int ret;

    for (auto port_oid: member_list) {

        if (!vpp_get_hwif_name(port_oid, 0, hwif_name)) {
            SWSS_LOG_WARN("VS hwif name not found for port %s", sai_serialize_object_id(port_oid).c_str());
            continue;
        }

        if (is_bind) {
            ret = vpp_acl_interface_bind(hwif_name.c_str(), acl_swindex, is_input);
            if (ret == 0) {
                ret = tunterm_acl_bindunbind(tbl_oid, true, hwif_name);
                if(ret == 0) {
                    m_acl_tbl_hw_ports_map[tbl_oid].push_back(hwif_name);
                }
            }
        } else {
            ret = vpp_acl_interface_unbind(hwif_name.c_str(), acl_swindex, is_input);
            if (ret == 0) {
                ret = tunterm_acl_bindunbind(tbl_oid, false, hwif_name);
                if(ret == 0) {
                    m_acl_tbl_hw_ports_map[tbl_oid].remove(hwif_name);
                }
            }
        }
        if (ret != 0) {
            auto sid = sai_serialize_object_id(tbl_oid);
            SWSS_LOG_ERROR("VS Acl tbl %s (swindex %u) %s failed status %d",
                           sid.c_str(), acl_swindex,
                           is_bind ? "bind": "unbind", ret);
            continue;
        }
        SWSS_LOG_NOTICE("ACL table %s %s port %s", sai_serialize_object_id(tbl_oid).c_str(),
                        is_bind ? "bound to": "unbound from", hwif_name.c_str());
    }
    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::getAclEntryStats(
    _In_ sai_object_id_t ace_cntr_oid,
    _In_ uint32_t attr_count,
    _Out_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_status_t status = SAI_STATUS_FAILURE;
    uint32_t acl_index, vpp_rule_base_index, num_rules;

    if (aclGetVppIndices(ace_cntr_oid, &acl_index, &vpp_rule_base_index, &num_rules) == SAI_STATUS_SUCCESS) {
        uint64_t total_packets = 0;
        uint64_t total_bytes = 0;

        // Query stats for each VPP rule created for this ACE and sum them
        for (uint32_t rule_offset = 0; rule_offset < num_rules; rule_offset++) {
            vpp_ace_stats_t ace_stats;
            uint32_t rule_index = vpp_rule_base_index + rule_offset;

            if (vpp_acl_ace_stats_query(acl_index, rule_index, &ace_stats) == 0) {
                total_packets += ace_stats.packets;
                total_bytes += ace_stats.bytes;
                SWSS_LOG_DEBUG("Rule offset %u (index %u): packets=%lu, bytes=%lu",
                              rule_offset, rule_index, ace_stats.packets, ace_stats.bytes);
            } else {
                SWSS_LOG_WARN("Failed to query stats for rule offset %u (index %u)",
                             rule_offset, rule_index);
                // Continue to query remaining rules even if one fails
            }
        }

        // Set the aggregated stats for all attributes
        for (uint32_t i = 0; i < attr_count; i++) {
            if (attr_list[i].id == SAI_ACL_COUNTER_ATTR_PACKETS) {
                attr_list[i].value.u64 = total_packets;
            } else if (attr_list[i].id == SAI_ACL_COUNTER_ATTR_BYTES) {
                attr_list[i].value.u64 = total_bytes;
            }
        }
        status = SAI_STATUS_SUCCESS;

        SWSS_LOG_INFO("ACE counter stats aggregated: %u rules, total_packets=%lu, total_bytes=%lu",
                     num_rules, total_packets, total_bytes);
    }

    return status;
}
