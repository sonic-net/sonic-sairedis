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
#include <tuple>
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
        return setAclRuleIpFamilyWildcard(*rule, AF_INET);

    case SAI_ACL_IP_TYPE_IPV6ANY:
        return setAclRuleIpFamilyWildcard(*rule, AF_INET6);

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

sai_status_t SwitchVpp::acl_rule_field_update(
    _In_ sai_acl_entry_attr_t          attr_id,
    _In_ const sai_attribute_value_t  *value,
    _Out_ vpp_acl_rule_t      *rule)
{
    SWSS_LOG_ENTER();

    sai_status_t status;

    assert(NULL != value);
    status = SAI_STATUS_SUCCESS;

    // Disabled qualifiers must not constrain the protocol or inferred family.
    if (attr_id >= SAI_ACL_ENTRY_ATTR_FIELD_START &&
        attr_id <= SAI_ACL_ENTRY_ATTR_FIELD_END && !value->aclfield.enable) {
        return SAI_STATUS_SUCCESS;
    }

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

    case SAI_ACL_ENTRY_ATTR_FIELD_OUTER_VLAN_ID:
        status = acl_rule_set_outer_vlan(value, rule);
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
        // MIRROR action is sticky: if a prior MIRROR_INGRESS/EGRESS already set the
        // rule to PERMIT_MIRROR, do not let PACKET_ACTION clobber it (attribute order
        // is not guaranteed). The mirror-action path forwards the original packet
        // regardless, so the combination behaves as "forward + clone".
        if (rule->action != VPP_ACL_ACTION_PERMIT_MIRROR) {
            acl_rule_set_action(value, rule);
        }
        break;

    case SAI_ACL_ENTRY_ATTR_ACTION_MIRROR_INGRESS:
    case SAI_ACL_ENTRY_ATTR_ACTION_MIRROR_EGRESS:
        if (value->aclaction.enable) {
            // SAI defines MIRROR_INGRESS/EGRESS as sai_object_list_t. We honor the
            // first session OID only (HLD restriction).
            const auto &objlist = value->aclaction.parameter.objlist;
            if (objlist.count == 0 || objlist.list == NULL) {
                SWSS_LOG_ERROR("Mirror action objlist is empty");
                return SAI_STATUS_INVALID_PARAMETER;
            }
            sai_object_id_t oid = objlist.list[0];
            auto it = m_mirror_sessions.find(oid);
            if (it == m_mirror_sessions.end()) {
                SWSS_LOG_ERROR("Mirror session %s not found for ACL mirror action", sai_serialize_object_id(oid).c_str());
                return SAI_STATUS_FAILURE;
            }
            if (it->second.sw_if_index > VPP_ACL_MIRROR_SW_IF_INDEX_MASK) {
                SWSS_LOG_ERROR("Mirror session %s sw_if_index %u exceeds packed-action limit %u",
                               sai_serialize_object_id(oid).c_str(), it->second.sw_if_index,
                               VPP_ACL_MIRROR_SW_IF_INDEX_MASK);
                return SAI_STATUS_INVALID_PARAMETER;
            }
            uint32_t mirror_flags =
                (attr_id == SAI_ACL_ENTRY_ATTR_ACTION_MIRROR_EGRESS) ?
                    VPP_ACL_MIRROR_F_DEFERRED : 0;
            rule->action = VPP_ACL_ACTION_PERMIT_MIRROR;
            rule->mirror_action = it->second.sw_if_index |
                (mirror_flags << VPP_ACL_MIRROR_FLAGS_SHIFT);
            SWSS_LOG_INFO("ACL mirror action set: session %s -> mirror_action 0x%08x stage %s (rule proto so far %d)",
                          sai_serialize_object_id(oid).c_str(), rule->mirror_action,
                          mirror_flags ? "egress" : "ingress",
                          rule->proto);
        }
        break;

    case SAI_ACL_ENTRY_ATTR_PRIORITY:
    case SAI_ACL_ENTRY_ATTR_TABLE_ID:
    case SAI_ACL_ENTRY_ATTR_ADMIN_STATE:
    case SAI_ACL_TABLE_ATTR_FIELD_ETHER_TYPE:
    case SAI_ACL_ENTRY_ATTR_ACTION_COUNTER:
        // NOOP here - these are either handled elsewhere or not currently applicable
        break;

    case SAI_ACL_ENTRY_ATTR_FIELD_IN_PORTS:
        /*
         * Not a per rule field: it names several ports, so it is applied by
         * fanning the generated rules out over them in fill_acl_rules().
         */
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

    std::string hwif_name = m_ifaceRegistry.resolveHwIfName(port_oid, vlan_id);
    if (hwif_name.empty()) {
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

        /*
         * get_max() stops at MAX_ACL_ATTRS, and the attribute store is keyed by
         * attribute name, so what survives is an alphabetical cut rather than
         * the attributes that matter. An entry that hits the cap may therefore
         * be missing qualifiers this code goes on to read - IN_PORTS among them,
         * which would leave a scoped entry looking unscoped. Report it: the cap
         * is not raised here, so this is the only signal that it was reached.
         */
        if (p_ace->attrs_count >= MAX_ACL_ATTRS) {
            SWSS_LOG_WARN("ACL entry %s has at least %u attributes, the most this code reads; "
                          "any beyond that were dropped by name order and are invisible to it",
                          sid.c_str(), MAX_ACL_ATTRS);
        }

        /*
         * get_max() copies list attributes via transfer_list(), which propagates
         * the source count but leaves dst.list = NULL when dst.count was 0 on
         * entry (see meta/SaiSerialize.cpp). Re-fetch the object lists we care
         * about with a pre-allocated buffer so the OIDs actually land in the ACE.
         */
        for (uint32_t i = 0; i < p_ace->attrs_count; i++) {
            sai_attribute_t *attr = &p_ace->attrs[i];
            sai_object_id_t *buf = NULL;

            if (attr->id == SAI_ACL_ENTRY_ATTR_ACTION_MIRROR_INGRESS) {
                buf = p_ace->mirror_ingress_objid_list;
            } else if (attr->id == SAI_ACL_ENTRY_ATTR_ACTION_MIRROR_EGRESS) {
                buf = p_ace->mirror_egress_objid_list;
            } else {
                continue;
            }

            attr->value.aclaction.parameter.objlist.list = buf;
            attr->value.aclaction.parameter.objlist.count = MAX_ACL_MIRROR_OIDS;

            sai_status_t st = get(SAI_OBJECT_TYPE_ACL_ENTRY, sid, 1, attr);
            if (st != SAI_STATUS_SUCCESS) {
                SWSS_LOG_WARN("Failed to re-fetch mirror action attr %d for %s: %s",
                              attr->id, sid.c_str(),
                              sai_serialize_status(st).c_str());
                attr->value.aclaction.parameter.objlist.list = NULL;
                attr->value.aclaction.parameter.objlist.count = 0;
            }
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
        if (acl_priority_attr_get(p_ace->attrs_count, p_ace->attrs,
                                  &p_ace->priority) != SAI_STATUS_SUCCESS) {
            SWSS_LOG_ERROR("No priority attribute on ACL entry %s, ordering it at 0; "
                           "its %u attributes may have been truncated at MAX_ACL_ATTRS (%u)",
                           sid.c_str(), p_ace->attrs_count, MAX_ACL_ATTRS);
        }

        ordered_aces.push_back({index, p_ace->priority, entry_id, false, 0, 0});
        p_ace++;
        index++;
    }

    if (status != SAI_STATUS_SUCCESS) {
        free(aces);
        aces = NULL;
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

void SwitchVpp::acl_table_get_ip_version(
    sai_object_id_t tbl_oid,
    bool &has_v4,
    bool &has_v6)
{
    SWSS_LOG_ENTER();

    has_v4 = false;
    has_v6 = false;

    auto sid = sai_serialize_object_id(tbl_oid);

    const uint32_t MAX_TBL_ATTRS = 64;
    // Must be zeroed: transfer_list() dereferences the destination's list/count
    // for list-valued table attributes, and garbage stack values crash syncd.
    sai_attribute_t attrs[MAX_TBL_ATTRS];
    memset(attrs, 0, sizeof(attrs));
    uint32_t count = 0;

    sai_status_t st = get_max(SAI_OBJECT_TYPE_ACL_TABLE, sid, MAX_TBL_ATTRS, &count, attrs);
    if (st != SAI_STATUS_SUCCESS && st != SAI_STATUS_BUFFER_OVERFLOW) {
        SWSS_LOG_WARN("Failed to read ACL table %s attrs to determine IP version: %s",
                      sid.c_str(), sai_serialize_status(st).c_str());
        return;
    }

    for (uint32_t i = 0; i < count; i++) {
        const sai_attribute_t *a = &attrs[i];

        switch (a->id) {
        case SAI_ACL_TABLE_ATTR_FIELD_SRC_IP:
        case SAI_ACL_TABLE_ATTR_FIELD_DST_IP:
        case SAI_ACL_TABLE_ATTR_FIELD_INNER_SRC_IP:
        case SAI_ACL_TABLE_ATTR_FIELD_INNER_DST_IP:
        case SAI_ACL_TABLE_ATTR_FIELD_ICMP_TYPE:
        case SAI_ACL_TABLE_ATTR_FIELD_ICMP_CODE:
            if (a->value.booldata) {
                has_v4 = true;
            }
            break;

        case SAI_ACL_TABLE_ATTR_FIELD_SRC_IPV6:
        case SAI_ACL_TABLE_ATTR_FIELD_DST_IPV6:
        case SAI_ACL_TABLE_ATTR_FIELD_INNER_SRC_IPV6:
        case SAI_ACL_TABLE_ATTR_FIELD_INNER_DST_IPV6:
        case SAI_ACL_TABLE_ATTR_FIELD_IPV6_NEXT_HEADER:
        case SAI_ACL_TABLE_ATTR_FIELD_ICMPV6_TYPE:
        case SAI_ACL_TABLE_ATTR_FIELD_ICMPV6_CODE:
            if (a->value.booldata) {
                has_v6 = true;
            }
            break;

        default:
            break;
        }
    }

    SWSS_LOG_INFO("ACL table %s IP version: has_v4=%d has_v6=%d",
                  sid.c_str(), has_v4, has_v6);
}

sai_status_t SwitchVpp::fill_acl_rules(
    sai_object_id_t tbl_oid,
    acl_tbl_entries_t *aces,
    std::list<ordered_ace_list_t> &ordered_aces,
    bool table_has_v4,
    bool table_has_v6,
    std::list<vpp_acl_rule_t> &acl_rules,
    std::list<vpp_tunterm_acl_rule_t> &tunterm_acl_rules,
    uint32_t &deferred_mirror_count)
{
    SWSS_LOG_ENTER();

    sai_status_t status = SAI_STATUS_SUCCESS;
    acl_tbl_entries_t *p_ace = NULL;
    uint32_t acl_rule_index = 0;
    uint32_t tunterm_rule_index = 0;
    deferred_mirror_count = 0;

    // An IN_PORTS scope is compiled into the rule's in_sw_if_index, which the
    // ACL plugin compares against sw_if_index[VLIB_RX] only for an ACL bound
    // inbound; bound outbound the same 5-tuple slot holds sw_if_index[VLIB_TX].
    // Programming an ingress scope into an egress table would therefore match
    // the egress port while still reporting the ingress port that was asked for.
    //
    // Resolved lazily, and only for an entry that actually carries IN_PORTS, so
    // a table with no scoped entry keeps behaving exactly as it did before this
    // feature.  An unreadable stage is treated as not-egress: the attribute is
    // MANDATORY_ON_CREATE on a table we are in the middle of programming, so
    // this should be unreachable, and failing here would take the whole table
    // down for a check that is only a guard.
    enum { STAGE_UNKNOWN, STAGE_EGRESS, STAGE_NOT_EGRESS } tbl_stage = STAGE_UNKNOWN;

    auto table_is_egress = [&]() -> bool {
        if (tbl_stage == STAGE_UNKNOWN) {
            sai_attribute_t attr;

            attr.id = SAI_ACL_TABLE_ATTR_ACL_STAGE;
            if (get(SAI_OBJECT_TYPE_ACL_TABLE, tbl_oid, 1, &attr) != SAI_STATUS_SUCCESS) {
                SWSS_LOG_ERROR("Failed to get stage of ACL table %s carrying an IN_PORTS entry, "
                               "assuming ingress",
                               sai_serialize_object_id(tbl_oid).c_str());
                tbl_stage = STAGE_NOT_EGRESS;
            } else {
                tbl_stage = (attr.value.s32 == SAI_ACL_STAGE_EGRESS) ? STAGE_EGRESS
                                                                     : STAGE_NOT_EGRESS;
            }
        }

        return (tbl_stage == STAGE_EGRESS);
    };

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
            int rule_family = AF_UNSPEC;

            status = getAclRuleIpFamily(*p_ace, rule_family);
            if (status != SAI_STATUS_SUCCESS) {
                SWSS_LOG_ERROR("ACL entry %s has conflicting IPv4 and IPv6 qualifiers",
                               sai_serialize_object_id(ace.ace_oid).c_str());
                return status;
            }

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
                        SWSS_LOG_ERROR("Failed to translate attr %d of ACL entry %s (ace index %u, "
                                       "priority %u), status: %d; aborting the whole table update",
                                       attr->id, sai_serialize_object_id(ace.ace_oid).c_str(),
                                       ace.index, ace.priority, status);
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

            if (rule.action == VPP_ACL_ACTION_PERMIT_MIRROR) {
                SWSS_LOG_INFO("Mirror ACL rule built (ace index %u, priority %u): proto=%d, "
                              "src_af=%d, dst_af=%d, mirror_action=0x%08x",
                              ace.index, ace.priority, rule.proto,
                              rule.src_prefix.sa_family, rule.dst_prefix.sa_family,
                              rule.mirror_action);
                if ((rule.mirror_action >> VPP_ACL_MIRROR_FLAGS_SHIFT) &
                    VPP_ACL_MIRROR_F_DEFERRED) {
                    deferred_mirror_count++;
                }
            }

            std::vector<vpp_acl_rule_t> variants;
            status = expandAclRuleVariants(rule, rule_family, table_has_v4, table_has_v6,
                                           port_proto != 0, variants);
            if (status != SAI_STATUS_SUCCESS) {
                SWSS_LOG_ERROR("ACL entry %s has incompatible family or protocol qualifiers",
                               sai_serialize_object_id(ace.ace_oid).c_str());
                return status;
            }
            acl_rules.insert(acl_rules.end(), variants.begin(), variants.end());
            rules_added = (uint32_t) variants.size();

            /*
             * SAI_ACL_ENTRY_ATTR_FIELD_IN_PORTS restricts the entry to the
             * ports it names. A VPP ACL rule matches one interface, so the
             * rules generated above are fanned out - one copy per named port -
             * the same way a port based entry with no protocol fans out into a
             * UDP and a TCP rule. They stay contiguous from
             * ace.vpp_rule_base_index, so the counter of the entry still sums
             * over ace.num_rules and needs no special handling.
             */
            std::set<std::string> in_hwifs;
            bool                  in_ports_scoped = false;

            /*
             * A scope that cannot be resolved comes back as an empty in_hwifs
             * rather than a status, so that it costs this entry its rules and
             * not the whole table. The status check is defensive only.
             */
            status = acl_entry_in_ports_get(p_ace, ace.ace_oid, in_ports_scoped, in_hwifs);

            if (status != SAI_STATUS_SUCCESS) {
                SWSS_LOG_ERROR("Failed to resolve IN_PORTS of ACL entry %s, status: %d",
                               sai_serialize_object_id(ace.ace_oid).c_str(), status);
                return SAI_STATUS_FAILURE;
            }

            if (in_ports_scoped) {

                if (table_is_egress()) {
                    /*
                     * The scope would silently become an egress one here, so
                     * emit no rule for this entry rather than a rule that
                     * enforces something other than what was asked for. The
                     * rest of the table still programs: failing it outright
                     * would take the table's unrelated entries down with it,
                     * and the entry cannot be un-programmed afterwards.
                     */
                    SWSS_LOG_ERROR("ACL entry %s in egress table %s carries IN_PORTS, which "
                                   "would match the egress interface rather than the ingress "
                                   "one; no rule is emitted for this entry",
                                   sai_serialize_object_id(ace.ace_oid).c_str(),
                                   sai_serialize_object_id(tbl_oid).c_str());
                    in_hwifs.clear();
                }

                std::list<vpp_acl_rule_t> base_rules;

                for (uint32_t n = 0; n < rules_added; n++) {
                    base_rules.push_front(acl_rules.back());
                    acl_rules.pop_back();
                }

                rules_added = 0;

                for (const auto &hwif_name : in_hwifs) {
                    for (auto scoped_rule : base_rules) {
                        snprintf(scoped_rule.in_hwif_name, sizeof(scoped_rule.in_hwif_name),
                                 "%s", hwif_name.c_str());
                        acl_rules.push_back(scoped_rule);
                        rules_added++;
                    }
                }

                SWSS_LOG_INFO("ACL entry %s scoped to %zu interface(s): %u rule(s)",
                              sai_serialize_object_id(ace.ace_oid).c_str(),
                              in_hwifs.size(), rules_added);
            }

            if (rules_added > (uint32_t) ACL_MAX_RULES_PER_ACE) {
                SWSS_LOG_WARN("ACL entry %s expanded to %u VPP rules; large expansions grow the "
                              "acl_add_replace message",
                              sai_serialize_object_id(ace.ace_oid).c_str(), rules_added);
            }

            ace.num_rules = rules_added;
            acl_rule_index += rules_added;

            SWSS_LOG_INFO("Regular ACE recorded: base_index=%u, num_rules=%u",
                         ace.vpp_rule_base_index, ace.num_rules);
        }
    }

    SWSS_LOG_INFO("fill_acl_rules complete: total %u acl_rules, %u tunterm_rules, "
                  "%u logical deferred mirror actions",
                  (uint32_t)acl_rules.size(), (uint32_t)tunterm_acl_rules.size(),
                  deferred_mirror_count);

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

sai_status_t SwitchVpp::acl_entry_in_ports_get(
    _In_ const acl_tbl_entries_t *ace,
    _In_ sai_object_id_t ace_oid,
    _Out_ bool &scoped,
    _Out_ std::set<std::string> &hwifs)
{
    SWSS_LOG_ENTER();

    scoped = false;
    hwifs.clear();

    const sai_attribute_t *found = NULL;

    for (uint32_t i = 0; i < ace->attrs_count; i++) {
        if (ace->attrs[i].id == SAI_ACL_ENTRY_ATTR_FIELD_IN_PORTS) {
            found = &ace->attrs[i];
            break;
        }
    }

    if (found == NULL || !found->value.aclfield.enable) {
        return SAI_STATUS_SUCCESS;
    }

    scoped = true;

    /*
     * The attributes were read with a zero capacity object list, so only its
     * count came back and its list is still null. Read the entry again, now
     * with somewhere to put the ports.
     */
    uint32_t count = found->value.aclfield.data.objlist.count;

    if (count == 0) {
        /*
         * The field is on but names no port, so no ingress interface can be in
         * the list and the entry matches nothing. Leaving hwifs empty makes the
         * caller emit no rule for it, which comes to the same thing.
         */
        SWSS_LOG_WARN("ACL entry %s has an enabled but empty IN_PORTS list",
                      sai_serialize_object_id(ace_oid).c_str());
        return SAI_STATUS_SUCCESS;
    }

    std::vector<sai_object_id_t> ports(count);
    sai_attribute_t              attr;

    attr.id = SAI_ACL_ENTRY_ATTR_FIELD_IN_PORTS;
    attr.value.aclfield.enable = true;
    attr.value.aclfield.data.objlist.count = count;
    attr.value.aclfield.data.objlist.list = ports.data();

    if (get(SAI_OBJECT_TYPE_ACL_ENTRY, ace_oid, 1, &attr) != SAI_STATUS_SUCCESS) {
        /*
         * The scope cannot be determined, so leave hwifs empty and let the
         * caller emit no rule for this entry. See the note below on why that
         * is preferred over failing the table.
         */
        SWSS_LOG_ERROR("Failed to read IN_PORTS list of ACL entry %s; no rule is emitted "
                       "for this entry",
                       sai_serialize_object_id(ace_oid).c_str());
        return SAI_STATUS_SUCCESS;
    }

    for (uint32_t i = 0; i < attr.value.aclfield.data.objlist.count; i++) {

        std::string hwif_name = m_ifaceRegistry.resolveHwIfName(ports[i], 0);
        if (hwif_name.empty()) {
            SWSS_LOG_WARN("No VPP interface for port %s named by IN_PORTS of ACL entry %s",
                          sai_serialize_object_id(ports[i]).c_str(),
                          sai_serialize_object_id(ace_oid).c_str());
            continue;
        }

        hwifs.insert(hwif_name);
    }

    /*
     * If nothing resolved, hwifs is left empty and the caller emits no rule for
     * this entry. That is deliberate, and it is the same outcome as the
     * degenerate cases above rather than a separate policy:
     *
     *   - Programming the entry unscoped would apply it to every port the table
     *     is bound to, which is the table-wide deny this whole change exists to
     *     remove.
     *   - Failing the table would abort AclTblConfig for every entry in it, and
     *     the table is shared: MuxOrch's drop rule and PFC watchdog's rules both
     *     live in IngressTableDrop. The failure would also be sticky, because
     *     createAclEntry has already committed the entry to the object store and
     *     to m_acl_tbl_rules_map before this runs and does not roll either back,
     *     so every later add or remove on the table would fail here again.
     *
     * Emitting no rule keeps the damage to the entry that could not be
     * resolved, and the error below is the signal.
     */
    if (hwifs.empty()) {
        SWSS_LOG_ERROR("None of the %u port(s) named by IN_PORTS of ACL entry %s resolve "
                       "to a VPP interface; no rule is emitted for this entry",
                       count, sai_serialize_object_id(ace_oid).c_str());
    }

    return SAI_STATUS_SUCCESS;
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
    uint32_t                            deferred_mirror_count = 0;

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

    // Determine the table's IP family so address-less rules (e.g. an Everflow
    // mirror rule matching only on L4 protocol) get emitted with the correct
    // IPv4/IPv6 family instead of defaulting to IPv4.
    bool table_has_v4 = false;
    bool table_has_v6 = false;
    acl_table_get_ip_version(tbl_oid, table_has_v4, table_has_v6);

    // Fill ACL rules - this returns converted rule lists
    sai_status_t fill_status = fill_acl_rules(tbl_oid, aces, ordered_aces, table_has_v4, table_has_v6,
                                              acl_rules, tunterm_acl_rules,
                                              deferred_mirror_count);
    if (fill_status != SAI_STATUS_SUCCESS) {
        SWSS_LOG_ERROR("ACL table %s not reprogrammed: rule translation failed (%s); previously "
                       "installed rules and deferred mirror count are retained",
                       sai_serialize_object_id(tbl_oid).c_str(),
                       sai_serialize_status(fill_status).c_str());
    }
    CHECK_STATUS_ACLTBLCONFIG(fill_status);

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

    // Apply the ACL configurations. acl is NULL when the table has no regular
    // rules left - e.g. the mux drop entry is deleted when a port transitions
    // standby -> active. In that case acl_add_replace() replaces the previously
    // programmed VPP ACL with an empty placeholder (see its acl == NULL branch),
    // so a stale rule such as deny 0/0->0/0 is not left bound to the mux ports
    // and silently dropping for-us/punt traffic. Only route a NULL acl through
    // acl_add_replace() when the table was already programmed in VPP, otherwise
    // there is nothing to clear and vpp_acl_add_replace() must not see a NULL.
    if (acl != NULL || m_acl_swindex_map.find(tbl_oid) != m_acl_swindex_map.end()) {
        status = acl_add_replace(acl, tbl_oid, aces, ordered_aces);
    } else {
        status = emptyAclCreate(tbl_oid);
    }

    if (status == SAI_STATUS_SUCCESS) {
        status = commitAclDeferredMirrorCount(tbl_oid, deferred_mirror_count);
    } else {
        SWSS_LOG_ERROR("Regular ACL update failed for table %s; installed deferred "
                       "mirror count remains unchanged (candidate %u)",
                       sai_serialize_object_id(tbl_oid).c_str(), deferred_mirror_count);
    }

    if (status == SAI_STATUS_SUCCESS && tunterm_acl != NULL) {
        status = tunterm_acl_add_replace(tunterm_acl, tbl_oid);
    } else if (status != SAI_STATUS_SUCCESS) {
        SWSS_LOG_ERROR("ACL plugin operation failed, skipping tunterm "
                       "configuration. status %d", status);
    }

    // ip2me: only once the ACL has actually been (re)programmed in VPP do we
    // record whether this table now carries a DROP/deny rule, so the in-memory
    // drop-table state can never diverge from what VPP is enforcing if the
    // programming above failed. A table with a deny rule can discard ip2me
    // (for-us) traffic on the l2-input arc before it is punted, which is
    // exactly what the ip2me node guards.
    if (status == SAI_STATUS_SUCCESS) {
        bool has_deny = false;
        for (const auto &rule : acl_rules) {
            if (rule.action == VPP_ACL_ACTION_API_DENY) {
                has_deny = true;
                break;
            }
        }
        ip2meUpdateDropTable(tbl_oid, has_deny);
    }

    cleanup_acl_tbl_config(aces, ordered_aces, acl, tunterm_acl);
    return status;
}

sai_status_t SwitchVpp::commitAclDeferredMirrorCount(
    _In_ sai_object_id_t tbl_oid,
    _In_ uint32_t deferred_mirror_count)
{
    SWSS_LOG_ENTER();

    uint32_t previous_count = 0;
    auto count_it = m_acl_deferred_mirror_count_map.find(tbl_oid);
    if (count_it != m_acl_deferred_mirror_count_map.end()) {
        previous_count = count_it->second;
    }

    m_acl_deferred_mirror_count -= previous_count;
    m_acl_deferred_mirror_count += deferred_mirror_count;

    if (deferred_mirror_count == 0) {
        m_acl_deferred_mirror_count_map.erase(tbl_oid);
    } else {
        m_acl_deferred_mirror_count_map[tbl_oid] = deferred_mirror_count;
    }

    bool enable = m_acl_deferred_mirror_count != 0;
    SWSS_LOG_INFO("ACL table %s installed deferred mirror count %u -> %u; "
                  "aggregate %u, desired feature state %s",
                  sai_serialize_object_id(tbl_oid).c_str(), previous_count,
                  deferred_mirror_count, m_acl_deferred_mirror_count,
                  enable ? "enabled" : "disabled");

    if (enable == m_acl_egress_mirror_feature_enabled) {
        return SAI_STATUS_SUCCESS;
    }

    int status = vpp_sonic_ext_egress_mirror_enable_disable(enable);
    if (status != 0) {
        SWSS_LOG_ERROR("Failed to set sonic_ext egress mirror feature to %s; "
                       "installed ACL counts retained for retry, status %d",
                       enable ? "enabled" : "disabled", status);
        return SAI_STATUS_FAILURE;
    }

    m_acl_egress_mirror_feature_enabled = enable;
    SWSS_LOG_NOTICE("sonic_ext egress mirror feature %s (aggregate deferred mirror count %u)",
                    enable ? "enabled" : "disabled", m_acl_deferred_mirror_count);
    return SAI_STATUS_SUCCESS;
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

    /*
     * A PBH table is an ACL table only as far as SAI is concerned. It goes to
     * the sonic_ext PBH feature instead of the ACL plugin, so no placeholder
     * ACL is created for it and it never enters m_acl_swindex_map.
     */
    if (m_pbh.isPbhTable(object_id))
    {
        sai_status_t status = m_pbh.tableCreate(object_id);

        if (status != SAI_STATUS_SUCCESS)
        {
            remove_internal(SAI_OBJECT_TYPE_ACL_TABLE, sid);
        }

        return status;
    }

    return emptyAclCreate(object_id);
}

sai_status_t SwitchVpp::aclTableRemove(
    _In_ const std::string &serializedObjectId)
{
    SWSS_LOG_ENTER();

    sai_object_id_t tbl_oid;

    sai_deserialize_object_id(serializedObjectId, tbl_oid);

    bool is_pbh = m_pbh.isPbhTable(tbl_oid);

    CHECK_STATUS(remove_internal(SAI_OBJECT_TYPE_ACL_TABLE, serializedObjectId));

    if (is_pbh)
    {
        return m_pbh.tableRemove(tbl_oid);
    }

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

    // ip2me: the table is going away -- clear its drop-table state and
    // re-evaluate any interfaces that still reference it (defensive; the
    // per-port unbind normally runs before the table is deleted).
    ip2meUpdateDropTable(tbl_oid, false);

    status = tunterm_acl_delete(tbl_oid, true);

    // The table is already gone from the SAI DB, so this is the last chance to drop
    // its contribution to the aggregate deferred mirror count.
    sai_status_t count_status = commitAclDeferredMirrorCount(tbl_oid, 0);
    if (count_status != SAI_STATUS_SUCCESS) {
        SWSS_LOG_ERROR("Failed to clear deferred mirror count for removed ACL table %s",
                       sai_serialize_object_id(tbl_oid).c_str());
    }

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

    /*
     * A PBH table is reprogrammed even when its entry list is empty: unlike
     * the ACL path, which leaves a placeholder rule behind, removing the last
     * PBH rule has to leave an empty table rather than the previous rule set.
     */
    if (m_pbh.isPbhTable(tbl_oid))
    {
        return m_pbh.tableConfig(tbl_oid);
    }

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
        remove_internal(SAI_OBJECT_TYPE_ACL_ENTRY, sid);
        return SAI_STATUS_FAILURE;
    }
    sai_status_t status;

    status = addRemoveAclEntrytoMap(object_id, tbl_oid, true);
    if (status == SAI_STATUS_SUCCESS) {
        status = AclAddRemoveCheck(tbl_oid);

        if (status != SAI_STATUS_SUCCESS) {
            addRemoveAclEntrytoMap(object_id, tbl_oid, false);
        }
    }

    if (status != SAI_STATUS_SUCCESS) {
        // acl_add_replace() swaps the table's whole rule set atomically, so a
        // failure means VPP never took this entry. Undo the state as well, so
        // it keeps describing what VPP holds. Left behind, the entry would
        // consume a slot of SAI_ACL_TABLE_ATTR_AVAILABLE_ACL_ENTRY that no
        // caller owns and no later remove can free, while the caller - having
        // seen the failure - never counts it in crm_stats_acl_entry_used.
        remove_internal(SAI_OBJECT_TYPE_ACL_ENTRY, sid);

        SWSS_LOG_ERROR("ACL entry %s in table %s create failed, status %d",
            sid.c_str(), sai_serialize_object_id(tbl_oid).c_str(), status);
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

    sai_status_t status = addRemoveAclEntrytoMap(entry_oid, tbl_oid, false);

    // A failure here means the entry was not listed under the table, so the rule
    // set VPP holds already excludes it and there is nothing to reprogram - the
    // removal has in effect already happened. addRemoveAclEntrytoMap() has
    // logged the inconsistency, and the object can go either way.
    if (status == SAI_STATUS_SUCCESS) {
        status = AclAddRemoveCheck(tbl_oid);

        if (status != SAI_STATUS_SUCCESS) {
            // acl_add_replace() swaps the table's whole rule set atomically, so
            // a failure means VPP is still enforcing this entry. Put it back and
            // report the failure, so the state keeps describing what VPP holds
            // and the caller keeps its accounting for the entry.
            addRemoveAclEntrytoMap(entry_oid, tbl_oid, true);

            SWSS_LOG_ERROR("ACL entry %s in table %s remove failed, status %d",
                sai_serialize_object_id(entry_oid).c_str(),
                sai_serialize_object_id(tbl_oid).c_str(),
                status);

            return status;
        }
    }

    remove_internal(SAI_OBJECT_TYPE_ACL_ENTRY, serializedObjectId);

    SWSS_LOG_NOTICE("ACL entry %s in table %s removed",
        sai_serialize_object_id(entry_oid).c_str(),
        sai_serialize_object_id(tbl_oid).c_str());

    return SAI_STATUS_SUCCESS;
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

    // Keep the original membership and desired ports until every attachment
    // has been updated. Per-interface bookkeeping continues to describe only
    // successful VPP operations, including when recovery also fails.
    auto mbr_it = m_acl_tbl_grp_mbr_map.find(tbl_grp_oid);
    const bool had_members = mbr_it != m_acl_tbl_grp_mbr_map.end();
    const std::list<sai_object_id_t> old_members = had_members ?
        mbr_it->second : std::list<sai_object_id_t>{};
    if (!is_add && std::find(old_members.begin(), old_members.end(), member_oid) == old_members.end()) {
        SWSS_LOG_ERROR("ACL group member %s not found in tbl group %s",
                       sai_serialize_object_id(member_oid).c_str(),
                       sai_serialize_object_id(tbl_grp_oid).c_str());
        return SAI_STATUS_FAILURE;
    }

    std::list<sai_object_id_t> ports_copy;
    auto ports_it = m_acl_tbl_grp_ports_map.find(tbl_grp_oid);
    if (ports_it != m_acl_tbl_grp_ports_map.end()) {
        ports_copy = ports_it->second;
    }

    auto restore_old = [&]() {
        if (had_members) {
            m_acl_tbl_grp_mbr_map[tbl_grp_oid] = old_members;
        } else {
            m_acl_tbl_grp_mbr_map.erase(tbl_grp_oid);
        }
        // Include ports already updated before the failure, and all bridged
        // children of the failing port. Retain associations even if VPP keeps
        // rejecting recovery, so the next member update can retry them.
        m_acl_tbl_grp_ports_map[tbl_grp_oid] = ports_copy;
        for (auto port_oid : ports_copy) {
            if (aclBindUnbindPort(port_oid, tbl_grp_oid, is_input, true) != SAI_STATUS_SUCCESS) {
                SWSS_LOG_ERROR("Failed to restore ACL group %s on port %s",
                               sai_serialize_object_id(tbl_grp_oid).c_str(),
                               sai_serialize_object_id(port_oid).c_str());
            }
        }
    };

    // Tentatively change membership, then replace each complete policy directly.
    auto &member_list = m_acl_tbl_grp_mbr_map[tbl_grp_oid];
    if (is_add) {
        if (std::find(member_list.begin(), member_list.end(), member_oid) == member_list.end()) {
            member_list.push_back(member_oid);
        }
    } else {
        member_list.remove(member_oid);
    }

    // Each target receives its complete new ordered list, with the permit last.
    for (auto port_oid : ports_copy) {
        sai_status_t status = aclBindUnbindPort(port_oid, tbl_grp_oid, is_input, true);
        if (status != SAI_STATUS_SUCCESS) {
            SWSS_LOG_ERROR("Failed to bind ACL group %s to port %s",
                           sai_serialize_object_id(tbl_grp_oid).c_str(),
                           sai_serialize_object_id(port_oid).c_str());
            restore_old();
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
        remove_internal(SAI_OBJECT_TYPE_ACL_TABLE_GROUP_MEMBER, sid);
        return SAI_STATUS_FAILURE;
    }
    sai_status_t status;

    status = addRemoveAclGrpMbr(object_id, tbl_grp_oid, true);
    if (status != SAI_STATUS_SUCCESS) {
        remove_internal(SAI_OBJECT_TYPE_ACL_TABLE_GROUP_MEMBER, sid);
    }

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

    if (status != SAI_STATUS_SUCCESS) {
        return status;
    }
    return remove_internal(SAI_OBJECT_TYPE_ACL_TABLE_GROUP_MEMBER, serializedObjectId);
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
            if (std::find(ports_list.begin(), ports_list.end(), port_oid) == ports_list.end()) {
                ports_list.push_back(port_oid);
            }
        } else {
            ports_list.remove(port_oid);
        }
    }
    return SAI_STATUS_SUCCESS;
}

/*
 * Generic port <-> ACL-table binding bookkeeping.
 *
 * updatePortAclTableBinding() maintains m_port_acl_tables: for each VPP
 * interface (hwif name) the set of ACL tables bound to it, tracked per
 * direction (ingress/egress). getPortsWithAclTable() is the reverse lookup
 * (table -> interfaces) for a direction. Neither is specific to a feature;
 * they are the shared port<->table map the ip2me hook (and future egress
 * features) build on.
 */
void SwitchVpp::updatePortAclTableBinding(
        _In_ const std::string &hwif_name,
        _In_ sai_object_id_t tbl_oid,
        _In_ bool is_input,
        _In_ bool is_bind)
{
    SWSS_LOG_ENTER();

    if (is_bind) {
        auto &tables = m_port_acl_tables[hwif_name];
        tables.updateTable(tbl_oid, is_input, true);
    } else {
        auto pit = m_port_acl_tables.find(hwif_name);
        if (pit != m_port_acl_tables.end()) {
            pit->second.updateTable(tbl_oid, is_input, false);
            if (pit->second.empty()) {
                m_port_acl_tables.erase(pit);
            }
        }
    }
}

std::vector<std::string> SwitchVpp::getPortsWithAclTable(
        _In_ sai_object_id_t tbl_oid,
        _In_ bool is_input)
{
    SWSS_LOG_ENTER();

    std::vector<std::string> ports;
    for (auto &kv : m_port_acl_tables) {
        const auto &tables = is_input ? kv.second.ingress : kv.second.egress;
        if (tables.count(tbl_oid)) {
            ports.push_back(kv.first);
        }
    }
    return ports;
}

/*
 * ip2me (receive-DPO check before ACL) -- sonic_ext plugin hook.
 *
 * Keep the sonic_ext ip2me feature enabled on exactly the VPP interfaces
 * that have an ingress ACL containing a DROP/deny rule bound, so ip2me
 * (destined-to-this-switch) traffic can bypass that drop ACL and still be
 * punted (mirrors hardware CoPP-before-ACL). The feature is a no-op on
 * every other interface and consumes zero ACL rules.
 *
 * State is split so the enable decision is robust to event ordering:
 *   - m_port_acl_tables     : per interface, the ACL tables bound to it in
 *                             each direction (generic; ip2me reads the ingress
 *                             set), so a table whose drop-ness flips after it
 *                             was bound can be re-evaluated.
 *   - m_ip2me_drop_tables   : tables that currently carry a deny rule
 *                             (updated by AclTblConfig on every rule change).
 *   - m_ip2me_enabled_ports : interfaces where the feature is programmed on,
 *                             so the vpp enable/disable stays idempotent.
 */
void SwitchVpp::ip2meRefreshPort(
        _In_ const std::string &hwif_name)
{
    SWSS_LOG_ENTER();

    // Desired state: enabled iff any ingress table bound to this interface
    // is currently a drop table.
    bool want_enable = false;
    if (sonicExtFeatureEnabled("ip2me")) {
        auto pit = m_port_acl_tables.find(hwif_name);
        if (pit != m_port_acl_tables.end()) {
            for (auto tbl_oid : pit->second.ingress) {
                if (m_ip2me_drop_tables.count(tbl_oid)) {
                    want_enable = true;
                    break;
                }
            }
        }
    }

    bool is_enabled = (m_ip2me_enabled_ports.count(hwif_name) != 0);
    if (want_enable == is_enabled) {
        return; // already in the desired state
    }

    int ret = vpp_sonic_ext_ip2me_enable_disable(hwif_name.c_str(), want_enable);
    if (ret != 0) {
        SWSS_LOG_ERROR("ip2me %s failed (%d) for interface %s",
                       want_enable ? "enable" : "disable", ret, hwif_name.c_str());
        return; // leave tracked state unchanged so a later refresh retries
    }

    if (want_enable) {
        m_ip2me_enabled_ports.insert(hwif_name);
    } else {
        m_ip2me_enabled_ports.erase(hwif_name);
    }
    SWSS_LOG_NOTICE("ip2me skip ACL %s on interface %s",
                    want_enable ? "enabled" : "disabled", hwif_name.c_str());
}

void SwitchVpp::ip2meUpdateDropTable(
        _In_ sai_object_id_t tbl_oid,
        _In_ bool has_deny)
{
    SWSS_LOG_ENTER();

    bool was_drop = (m_ip2me_drop_tables.count(tbl_oid) != 0);
    if (has_deny == was_drop) {
        return; // drop-ness unchanged
    }

    if (has_deny) {
        m_ip2me_drop_tables.insert(tbl_oid);
    } else {
        m_ip2me_drop_tables.erase(tbl_oid);
    }

    // Drop-ness flipped: re-evaluate every interface that currently has this
    // table bound on ingress (the bind may have happened before the rules were
    // added, or the last deny rule was just removed).
    for (const auto &hwif_name : getPortsWithAclTable(tbl_oid, /*is_input*/ true)) {
        ip2meRefreshPort(hwif_name);
    }
}

void SwitchVpp::aclRecordInterfacePolicy(
        _In_ const std::string &hwif_name,
        _In_ const PortAclTables &policy)
{
    auto it = m_port_acl_tables.find(hwif_name);
    const auto previous = it == m_port_acl_tables.end() ? PortAclTables{} : it->second;
    for (bool is_input : {true, false}) {
        const auto &old_tables = is_input ? previous.ingress : previous.egress;
        const auto &new_tables = is_input ? policy.ingress : policy.egress;
        for (auto table : old_tables) {
            if (!new_tables.count(table)) {
                m_acl_tunterm_pending[std::make_tuple(hwif_name, table, is_input)] = false;
            }
        }
        for (auto table : new_tables) {
            if (!old_tables.count(table)) {
                m_acl_tunterm_pending[std::make_tuple(hwif_name, table, is_input)] = true;
            }
        }
    }
    std::set<sai_object_id_t> tables = previous.ingress;
    tables.insert(previous.egress.begin(), previous.egress.end());
    tables.insert(policy.ingress.begin(), policy.ingress.end());
    tables.insert(policy.egress.begin(), policy.egress.end());
    for (auto table : tables) {
        auto &ports = m_acl_tbl_hw_ports_map[table];
        if (policy.ingress.count(table) || policy.egress.count(table)) {
            if (std::find(ports.begin(), ports.end(), hwif_name) == ports.end()) {
                ports.push_back(hwif_name);
            }
        } else {
            ports.remove(hwif_name);
        }
    }
    if (policy.empty()) {
        m_port_acl_tables.erase(hwif_name);
    } else {
        m_port_acl_tables[hwif_name] = policy;
    }
    ip2meRefreshPort(hwif_name);
}

sai_status_t SwitchVpp::aclReadInterfacePolicy(
        _In_ const std::string &hwif_name)
{
    uint32_t input[255] = {}, output[255] = {};
    uint32_t input_count = 0, output_count = 0;
    m_acl_interface_state_unknown.insert(hwif_name);
    if (vpp_acl_interface_get_acl_list(hwif_name.c_str(), input, &input_count,
                                     output, &output_count) != 0 ||
        input_count > 255 || output_count > 255 - input_count) {
        SWSS_LOG_ERROR("Cannot read ACL lists on %s; installed state remains unverified", hwif_name.c_str());
        return SAI_STATUS_FAILURE;
    }
    PortAclTables actual;
    for (bool is_input : {true, false}) {
        const uint32_t *indices = is_input ? input : output;
        const auto count = is_input ? input_count : output_count;
        std::set<uint32_t> seen;
        for (uint32_t i = 0; i < count; ++i) {
            if (!seen.insert(indices[i]).second) {
                return SAI_STATUS_FAILURE;
            }
            if (m_acl_default_created && indices[i] == m_acl_default_swindex) {
                // Backend-owned default permits always finish their direction.
                if (i + 1 != count) {
                    return SAI_STATUS_FAILURE;
                }
                (is_input ? actual.ingressDefault : actual.egressDefault) = true;
                continue;
            }
            auto table = std::find_if(m_acl_swindex_map.begin(), m_acl_swindex_map.end(),
                [indices, i](const std::pair<const sai_object_id_t, uint32_t> &entry) {
                    return entry.second == indices[i];
                });
            if (table == m_acl_swindex_map.end()) {
                SWSS_LOG_ERROR("Unknown ACL index %u on %s", indices[i], hwif_name.c_str());
                return SAI_STATUS_FAILURE;
            }
            actual.updateTable(table->first, is_input, true);
        }
    }
    aclRecordInterfacePolicy(hwif_name, actual);
    m_acl_interface_state_unknown.erase(hwif_name);
    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::aclApplyInterfacePolicy(
        _In_ const std::string &hwif_name,
        _In_ const PortAclTables &desired,
        _In_ bool change_input,
        _In_ bool change_output)
{
    SWSS_LOG_ENTER();
    if ((!change_input || !change_output) && m_acl_interface_state_unknown.count(hwif_name)) {
        CHECK_STATUS(aclReadInterfacePolicy(hwif_name));
    }
    auto it = m_port_acl_tables.find(hwif_name);
    const auto previous = it == m_port_acl_tables.end() ? PortAclTables{} : it->second;
    PortAclTables wanted = previous;
    for (bool is_input : {true, false}) {
        if (!(is_input ? change_input : change_output)) {
            continue;
        }
        auto &order = is_input ? wanted.ingressOrder : wanted.egressOrder;
        auto &tables = is_input ? wanted.ingress : wanted.egress;
        order.clear();
        tables.clear();
        const auto &requested = is_input ? desired.ingressOrder : desired.egressOrder;
        for (auto table : requested) {
            if (tables.insert(table).second) {
                order.push_back(table);
            }
        }
        (is_input ? wanted.ingressDefault : wanted.egressDefault) = is_input ?
            (desired.ingressDefault || desired.ingressDefaultPending) :
            (desired.egressDefault || desired.egressDefaultPending);
        (is_input ? wanted.ingressDefaultPending : wanted.egressDefaultPending) = false;
    }
    const size_t count = wanted.ingressOrder.size() + wanted.egressOrder.size() +
        (wanted.ingressDefault ? 1u : 0u) + (wanted.egressDefault ? 1u : 0u);
    if (count > 255) {
        SWSS_LOG_ERROR("ACL lists on %s exceed the VPP limit of 255 total indices", hwif_name.c_str());
        return SAI_STATUS_INVALID_PARAMETER;
    }
    if (wanted.ingressDefault || wanted.egressDefault) {
        CHECK_STATUS(aclDefaultCreate());
    }
    std::vector<uint32_t> input, output;
    for (bool is_input : {true, false}) {
        auto &indices = is_input ? input : output;
        const auto &order = is_input ? wanted.ingressOrder : wanted.egressOrder;
        std::set<uint32_t> seen;
        for (auto table : order) {
            auto index = m_acl_swindex_map.find(table);
            if (index == m_acl_swindex_map.end() || !seen.insert(index->second).second) {
                return SAI_STATUS_INVALID_PARAMETER;
            }
            indices.push_back(index->second);
        }
        if (is_input ? wanted.ingressDefault : wanted.egressDefault) {
            if (!seen.insert(m_acl_default_swindex).second) {
                return SAI_STATUS_INVALID_PARAMETER;
            }
            indices.push_back(m_acl_default_swindex);
        }
    }
    const bool changed = wanted.ingressOrder != previous.ingressOrder ||
        wanted.egressOrder != previous.egressOrder ||
        wanted.ingressDefault != previous.ingressDefault ||
        wanted.egressDefault != previous.egressDefault;
    if (changed || m_acl_interface_state_unknown.count(hwif_name)) {
        if (vpp_acl_interface_set_acl_list(hwif_name.c_str(), input.data(),
                static_cast<uint32_t>(input.size()), output.data(),
                static_cast<uint32_t>(output.size())) != 0) {
            // VPP replaces output then input. A failed reply (or timeout) does
            // not establish which direction committed; record a verified dump.
            aclReadInterfacePolicy(hwif_name);
            SWSS_LOG_ERROR("ACL list replacement failed on %s", hwif_name.c_str());
            return SAI_STATUS_FAILURE;
        }
        aclRecordInterfacePolicy(hwif_name, wanted);
        m_acl_interface_state_unknown.erase(hwif_name);
    } else {
        // A prior best-effort ip2me feature update may still need retrying.
        ip2meRefreshPort(hwif_name);
    }
    // The regular lists are now known. Ancillary programming has its own API
    // and can fail independently; keep that work until a later policy retry.
    const auto pending = m_acl_tunterm_pending;
    sai_status_t status = SAI_STATUS_SUCCESS;
    for (const auto &entry : pending) {
        const bool is_input = std::get<2>(entry.first);
        if (std::get<0>(entry.first) != hwif_name ||
            !(is_input ? change_input : change_output)) {
            continue;
        }
        const auto table = std::get<1>(entry.first);
        // Tunnel-termination bindings have no direction: retain them while
        // either regular direction still uses this table.
        const bool is_bind = wanted.ingress.count(table) || wanted.egress.count(table);
        if (tunterm_acl_bindunbind(table, is_bind, hwif_name) == SAI_STATUS_SUCCESS) {
            m_acl_tunterm_pending.erase(entry.first);
        } else {
            m_acl_tunterm_pending[entry.first] = is_bind;
            status = SAI_STATUS_FAILURE;
        }
    }
    return status;
}

sai_status_t SwitchVpp::aclBindInterfaceDefault(
        _In_ const std::string &hwif_name,
        _In_ bool is_input,
        _In_ bool is_bind)
{
    if (m_acl_interface_state_unknown.count(hwif_name)) {
        CHECK_STATUS(aclReadInterfacePolicy(hwif_name));
    }
    auto it = m_port_acl_tables.find(hwif_name);
    auto desired = it == m_port_acl_tables.end() ? PortAclTables{} : it->second;
    (is_input ? desired.ingressDefault : desired.egressDefault) = is_bind;
    (is_input ? desired.ingressDefaultPending : desired.egressDefaultPending) = false;
    return aclApplyInterfacePolicy(hwif_name, desired, is_input, !is_input);
}

sai_status_t SwitchVpp::aclBindInterfaceTable(
        _In_ const std::string &hwif_name,
        _In_ sai_object_id_t tbl_oid,
        _In_ bool is_input,
        _In_ bool is_bind)
{
    if (m_acl_interface_state_unknown.count(hwif_name)) {
        CHECK_STATUS(aclReadInterfacePolicy(hwif_name));
    }
    auto it = m_port_acl_tables.find(hwif_name);
    auto desired = it == m_port_acl_tables.end() ? PortAclTables{} : it->second;
    desired.updateTable(tbl_oid, is_input, is_bind);
    (is_input ? desired.ingressDefault : desired.egressDefault) =
        !(is_input ? desired.ingressOrder : desired.egressOrder).empty();
    (is_input ? desired.ingressDefaultPending : desired.egressDefaultPending) = false;
    return aclApplyInterfacePolicy(hwif_name, desired, is_input, !is_input);
}

sai_status_t SwitchVpp::aclInheritInterfacePolicy(
        _In_ const std::string &parent_hwif,
        _In_ const std::string &child_hwif)
{
    SWSS_LOG_ENTER();
    if (m_acl_interface_state_unknown.count(parent_hwif)) {
        CHECK_STATUS(aclReadInterfacePolicy(parent_hwif));
    }
    auto it = m_port_acl_tables.find(parent_hwif);
    const auto desired = it == m_port_acl_tables.end() ? PortAclTables{} : it->second;
    if (aclApplyInterfacePolicy(child_hwif, desired) != SAI_STATUS_SUCCESS) {
        aclRemoveInterfacePolicy(child_hwif);
        return SAI_STATUS_FAILURE;
    }
    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::aclRemoveInterfacePolicy(
        _In_ const std::string &hwif_name)
{
    return aclApplyInterfacePolicy(hwif_name, PortAclTables{});
}

void SwitchVpp::aclForgetInterfacePolicy(
        _In_ const std::string &hwif_name)
{
    SWSS_LOG_ENTER();
    m_port_acl_tables.erase(hwif_name);
    m_acl_interface_state_unknown.erase(hwif_name);
    m_ip2me_enabled_ports.erase(hwif_name);
    for (auto &entry : m_acl_tbl_hw_ports_map) {
        entry.second.remove(hwif_name);
    }
    for (auto it = m_acl_tunterm_pending.begin(); it != m_acl_tunterm_pending.end();) {
        if (std::get<0>(it->first) == hwif_name) {
            it = m_acl_tunterm_pending.erase(it);
        } else {
            ++it;
        }
    }
}

sai_status_t SwitchVpp::aclReconcileGroupPbh(
        _In_ sai_object_id_t port_oid,
        _In_ sai_object_id_t group_oid,
        _In_ const std::set<sai_object_id_t> &desired)
{
    const auto key = std::make_pair(port_oid, group_oid);
    auto found = m_acl_group_pbh_tables.find(key);
    if (found == m_acl_group_pbh_tables.end() && desired.empty()) {
        return SAI_STATUS_SUCCESS;
    }
    const auto hwif = m_ifaceRegistry.resolveHwIfName(port_oid, 0);
    if (hwif.empty()) {
        return SAI_STATUS_FAILURE;
    }
    auto &actual = m_acl_group_pbh_tables[key];
    const auto previous = actual;
    sai_status_t status = SAI_STATUS_SUCCESS;
    for (auto table : previous) {
        if (desired.count(table)) {
            continue;
        }
        // A table shared by another logical group remains attached there.
        const bool shared = std::any_of(m_acl_group_pbh_tables.begin(), m_acl_group_pbh_tables.end(),
            [port_oid, group_oid, table](const std::pair<const std::pair<sai_object_id_t, sai_object_id_t>,
                                             std::set<sai_object_id_t>> &entry) {
                return entry.first.first == port_oid && entry.first.second != group_oid &&
                    entry.second.count(table);
            });
        if (shared || m_pbh.bindUnbindPort(hwif, table, false) == SAI_STATUS_SUCCESS) {
            actual.erase(table);
        } else {
            status = SAI_STATUS_FAILURE;
        }
    }
    for (auto table : desired) {
        if (actual.count(table)) {
            continue;
        }
        if (m_pbh.bindUnbindPort(hwif, table, true) == SAI_STATUS_SUCCESS) {
            actual.insert(table);
        } else {
            status = SAI_STATUS_FAILURE;
        }
    }
    if (actual.empty()) {
        m_acl_group_pbh_tables.erase(key);
    }
    return status;
}

sai_status_t SwitchVpp::aclRestorePbhRegularPolicy(
        _In_ sai_object_id_t port_oid,
        _In_ sai_object_id_t group_oid,
        _In_ bool is_input)
{
    const auto key = std::make_tuple(port_oid, group_oid, is_input);
    auto pending = m_acl_pbh_regular_restore.find(key);
    if (pending == m_acl_pbh_regular_restore.end()) {
        return SAI_STATUS_SUCCESS;
    }
    const auto parent = m_ifaceRegistry.resolveHwIfName(port_oid, 0);
    const auto parent_policy = pending->second.find(parent);
    sai_status_t status = SAI_STATUS_SUCCESS;
    for (const auto &hwif : m_ifaceRegistry.resolveAclInterfaces(port_oid)) {
        auto policy = pending->second.find(hwif);
        if (policy == pending->second.end()) {
            // A child created during incomplete recovery inherits the original
            // parent's required policy when the accepted group is retried.
            policy = parent_policy;
        }
        if (policy == pending->second.end() ||
            aclApplyInterfacePolicy(hwif, policy->second, is_input, !is_input) != SAI_STATUS_SUCCESS) {
            status = SAI_STATUS_FAILURE;
        }
    }
    if (status == SAI_STATUS_SUCCESS) {
        m_acl_pbh_regular_restore.erase(pending);
    }
    return status;
}

sai_status_t SwitchVpp::aclSetPortTableGroup(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t port_oid,
        _In_ sai_attr_id_t attr_id,
        _In_ sai_object_id_t group_oid,
        _In_ bool is_input)
{
    SWSS_LOG_ENTER();

    // get() reports ITEM_NOT_FOUND for both an absent object and an omitted
    // attribute. Only an existing PORT/LAG may use the null ACL default.
    const auto &objects = m_objectHash.at(object_type);
    if (objects.find(sai_serialize_object_id(port_oid)) == objects.end()) {
        return SAI_STATUS_ITEM_NOT_FOUND;
    }

    sai_attribute_t old_attr = {};
    old_attr.id = attr_id;
    sai_status_t status = get(object_type, port_oid, 1, &old_attr);
    if (status != SAI_STATUS_SUCCESS && status != SAI_STATUS_ITEM_NOT_FOUND &&
        status != SAI_STATUS_NOT_IMPLEMENTED) {
        return status;
    }
    // PORT/LAG ACL attributes have the SAI null ACL default when omitted.
    sai_object_id_t old_group = status == SAI_STATUS_SUCCESS ? old_attr.value.oid : SAI_NULL_OBJECT_ID;
    if (old_group == group_oid) {
        return aclBindUnbindPort(port_oid, group_oid, is_input, group_oid != SAI_NULL_OBJECT_ID);
    }

    auto is_pbh_only = [&](sai_object_id_t group) {
        auto members = m_acl_tbl_grp_mbr_map.find(group);
        if (members == m_acl_tbl_grp_mbr_map.end() || members->second.empty()) {
            return false;
        }
        for (auto member : members->second) {
            sai_attribute_t table = {};
            table.id = SAI_ACL_TABLE_GROUP_MEMBER_ATTR_ACL_TABLE_ID;
            if (get(SAI_OBJECT_TYPE_ACL_TABLE_GROUP_MEMBER, member, 1, &table) != SAI_STATUS_SUCCESS ||
                !m_pbh.isPbhTable(table.value.oid)) {
                return false;
            }
        }
        return true;
    };
    const bool old_pbh_only = is_pbh_only(old_group);
    const bool new_pbh_only = is_pbh_only(group_oid);
    const auto restore_key = std::make_tuple(port_oid, old_group, is_input);
    std::map<std::string, PortAclTables> old_regular;
    if (old_pbh_only) {
        auto pending = m_acl_pbh_regular_restore.find(restore_key);
        if (pending != m_acl_pbh_regular_restore.end()) {
            old_regular = pending->second;
        } else {
            for (const auto &hwif : m_ifaceRegistry.resolveAclInterfaces(port_oid)) {
                if (m_acl_interface_state_unknown.count(hwif)) {
                    CHECK_STATUS(aclReadInterfacePolicy(hwif));
                }
                auto policy = m_port_acl_tables.find(hwif);
                old_regular[hwif] = policy == m_port_acl_tables.end() ? PortAclTables{} : policy->second;
            }
        }
    }
    auto restore_old = [&]() {
        if (aclBindUnbindPort(port_oid, old_group, is_input,
                             old_group != SAI_NULL_OBJECT_ID) != SAI_STATUS_SUCCESS) {
            SWSS_LOG_ERROR("Failed to restore ACL group %s on port %s after a failed update",
                           sai_serialize_object_id(old_group).c_str(), sai_serialize_object_id(port_oid).c_str());
        }
        // This desired snapshot must outlive the failed operation if VPP
        // also rejects restoring the independent regular policy.
        if (old_pbh_only) {
            m_acl_pbh_regular_restore[restore_key] = old_regular;
            if (aclRestorePbhRegularPolicy(port_oid, old_group, is_input) != SAI_STATUS_SUCCESS) {
                SWSS_LOG_ERROR("Regular ACL restoration remains pending for PBH group %s",
                               sai_serialize_object_id(old_group).c_str());
            }
        }
    };

    // PBH is programmed outside the ACL plugin. Replacing a group must also
    // remove its old PBH contribution; regular lists never detach first.
    status = aclReconcileGroupPbh(port_oid, old_group, {});
    if (status == SAI_STATUS_SUCCESS && new_pbh_only && !old_pbh_only && old_group != SAI_NULL_OBJECT_ID) {
        for (const auto &hwif : m_ifaceRegistry.resolveAclInterfaces(port_oid)) {
            if (aclApplyInterfacePolicy(hwif, PortAclTables{}, is_input, !is_input) != SAI_STATUS_SUCCESS) {
                status = SAI_STATUS_FAILURE;
            }
        }
    }
    if (status == SAI_STATUS_SUCCESS) {
        status = aclBindUnbindPort(port_oid, group_oid != SAI_NULL_OBJECT_ID ? group_oid : old_group,
                                  is_input, group_oid != SAI_NULL_OBJECT_ID);
    }
    if (status != SAI_STATUS_SUCCESS) {
        if (group_oid != SAI_NULL_OBJECT_ID &&
            aclReconcileGroupPbh(port_oid, group_oid, {}) != SAI_STATUS_SUCCESS) {
            SWSS_LOG_ERROR("PBH cleanup remains pending for rejected ACL group %s",
                           sai_serialize_object_id(group_oid).c_str());
        }
        restore_old();
        if (group_oid != SAI_NULL_OBJECT_ID) {
            // Rejected owners cannot take the port back through later member
            // updates. Failed PBH cleanup stays in successful-attachment state.
            addRemovePortTblGrp(port_oid, group_oid, false);
        }
        return status;
    }
    m_acl_pbh_regular_restore.erase(restore_key);
    if (old_group != SAI_NULL_OBJECT_ID && old_group != group_oid) {
        addRemovePortTblGrp(port_oid, old_group, false);
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

    if (is_bind) {
        addRemovePortTblGrp(port_oid, tbl_grp_oid, true);
    }

    auto it = m_acl_tbl_grp_mbr_map.find(tbl_grp_oid);

    if (it == m_acl_tbl_grp_mbr_map.end()) {
        auto sid = sai_serialize_object_id(tbl_grp_oid);
        SWSS_LOG_INFO("ACL tbl group with id %s not found", sid.c_str());
        /*
         * The tbl group is not created until a group member is added. The bind port
         * will be called later when a group member is added.
         */
    }

    const std::list<sai_object_id_t> member_list = it != m_acl_tbl_grp_mbr_map.end() ?
        it->second : std::list<sai_object_id_t>{};

    auto interfaces = m_ifaceRegistry.resolveAclInterfaces(port_oid);

    if (interfaces.empty() && !member_list.empty()) {
        SWSS_LOG_WARN("VS hwif name not found for port %s", sai_serialize_object_id(port_oid).c_str());
        return SAI_STATUS_FAILURE;
    }

    // Build a vector of (priority, acl_swindex, tbl_oid) for sorting
    std::vector<std::tuple<uint32_t, uint32_t, sai_object_id_t>> sorted_members;
    std::vector<sai_object_id_t> pbh_tables;
    bool has_acl_members = false;

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

        /*
         * A PBH table attaches to the interface through the sonic_ext PBH
         * feature instead of the ACL plugin, so it is handled here and kept
         * out of the priority-sorted ACL list below: PBH runs on the IP
         * feature arc after the ACL hook, not as another ACL in the chain.
         */
        if (m_pbh.isPbhTable(tbl_oid)) {
            pbh_tables.push_back(tbl_oid);
            continue;
        }

        has_acl_members = true;
        if (!is_bind) {
            continue;
        }

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

        sorted_members.push_back({priority, acl_swindex, tbl_oid});
    }

    sai_status_t pbh_status = aclRestorePbhRegularPolicy(port_oid, tbl_grp_oid, is_input);
    const auto pbh_snapshot = m_acl_group_pbh_tables;
    for (const auto &entry : pbh_snapshot) {
        if (entry.first.first != port_oid || entry.first.second == tbl_grp_oid) {
            continue;
        }
        auto owners = m_acl_tbl_grp_ports_map.find(entry.first.second);
        if (owners == m_acl_tbl_grp_ports_map.end() ||
            std::find(owners->second.begin(), owners->second.end(), port_oid) == owners->second.end()) {
            if (aclReconcileGroupPbh(port_oid, entry.first.second, {}) != SAI_STATUS_SUCCESS) {
                pbh_status = SAI_STATUS_FAILURE;
            }
        }
    }
    const auto pbh_key = std::make_pair(port_oid, tbl_grp_oid);
    const bool had_pbh = m_acl_group_pbh_tables.count(pbh_key) != 0;
    const std::set<sai_object_id_t> wanted_pbh = is_bind ?
        std::set<sai_object_id_t>(pbh_tables.begin(), pbh_tables.end()) : std::set<sai_object_id_t>{};
    if (aclReconcileGroupPbh(port_oid, tbl_grp_oid, wanted_pbh) != SAI_STATUS_SUCCESS) {
        pbh_status = SAI_STATUS_FAILURE;
    }
    // PBH-only groups never replace another group's regular ACL chain.
    // A last PBH member removal likewise only removes that PBH contribution.
    if (!has_acl_members && (!pbh_tables.empty() || had_pbh)) {
        if (!is_bind && pbh_status == SAI_STATUS_SUCCESS) {
            addRemovePortTblGrp(port_oid, tbl_grp_oid, false);
        }
        return pbh_status;
    }

    // Sort by priority in descending order (higher priority first)
    std::sort(sorted_members.begin(), sorted_members.end(),
              [](const std::tuple<uint32_t, uint32_t, sai_object_id_t>& a,
                 const std::tuple<uint32_t, uint32_t, sai_object_id_t>& b) {
                  return std::get<0>(a) > std::get<0>(b);
              });

    PortAclTables desired;
    for (const auto &member : sorted_members) {
        desired.updateTable(std::get<2>(member), is_input, true);
    }
    (is_input ? desired.ingressDefault : desired.egressDefault) = !sorted_members.empty();
    sai_status_t status = pbh_status;
    for (const auto &hwif_name : interfaces) {
        if (aclApplyInterfacePolicy(hwif_name, desired, is_input, !is_input) != SAI_STATUS_SUCCESS) {
            status = SAI_STATUS_FAILURE;
        }
    }
    if (!is_bind && status == SAI_STATUS_SUCCESS) {
        addRemovePortTblGrp(port_oid, tbl_grp_oid, false);
    }
    return status;
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
    const auto member_list = it->second;
    sai_status_t status = SAI_STATUS_SUCCESS;
    for (auto port_oid : member_list) {
        auto interfaces = m_ifaceRegistry.resolveAclInterfaces(port_oid);
        if (interfaces.empty()) {
            SWSS_LOG_WARN("VS hwif name not found for port %s", sai_serialize_object_id(port_oid).c_str());
            status = SAI_STATUS_FAILURE;
            continue;
        }
        for (const auto &hwif_name : interfaces) {
            if (aclBindInterfaceTable(hwif_name, tbl_oid, is_input, is_bind) != SAI_STATUS_SUCCESS) {
                status = SAI_STATUS_FAILURE;
            }
        }
    }
    return status;
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

            if (vpp_rule_stats_query(VPP_RULE_STATS_ACL, acl_index, rule_index, &ace_stats) == 0) {
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
