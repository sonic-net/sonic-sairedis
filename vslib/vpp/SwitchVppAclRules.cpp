#include "sai.h"
#include "SwitchVppAcl.h"

#include <cstring>
#include <sys/socket.h>

namespace
{
    bool vpp_ip_addr_family_is_set(const vpp_ip_addr_t *addr)
    {
        return addr->sa_family == AF_INET || addr->sa_family == AF_INET6;
    }

    void set_any_addr(vpp_ip_addr_t *addr, int family)
    {
        memset(addr, 0, sizeof(*addr));
        addr->sa_family = family;
    }
}

sai_status_t saivs::setAclRuleIpFamilyWildcard(vpp_acl_rule_t &rule, int family)
{
    if (family != AF_INET && family != AF_INET6) {
        return SAI_STATUS_INVALID_PARAMETER;
    }
    for (const auto *prefix : {&rule.src_prefix, &rule.dst_prefix}) {
        if (vpp_ip_addr_family_is_set(prefix) && prefix->sa_family != family) {
            return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    if (!vpp_ip_addr_family_is_set(&rule.src_prefix)) {
        set_any_addr(&rule.src_prefix, family);
        set_any_addr(&rule.src_prefix_mask, family);
    }
    if (!vpp_ip_addr_family_is_set(&rule.dst_prefix)) {
        set_any_addr(&rule.dst_prefix, family);
        set_any_addr(&rule.dst_prefix_mask, family);
    }
    return SAI_STATUS_SUCCESS;
}

sai_status_t saivs::acl_rule_set_outer_vlan(
    const sai_attribute_value_t *value,
    vpp_acl_rule_t *rule)
{
    if (!value || !rule) {
        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (!value->aclfield.enable) {
        rule->outer_vlan_id = 0;
        rule->outer_vlan_id_mask = 0;
        return SAI_STATUS_SUCCESS;
    }

    if (value->aclfield.data.u16 > 0x0fff || value->aclfield.mask.u16 > 0x0fff) {
        return SAI_STATUS_INVALID_PARAMETER;
    }

    rule->outer_vlan_id_mask = value->aclfield.mask.u16;
    rule->outer_vlan_id = value->aclfield.data.u16 & rule->outer_vlan_id_mask;
    return SAI_STATUS_SUCCESS;
}

/*
 * Resolve entry constraints before translating fields so their order cannot
 * hide conflicting prefixes, IP_TYPE, EtherType or protocol-specific matches.
 */
sai_status_t saivs::getAclRuleIpFamily(
    const acl_tbl_entries_t &ace,
    int &family)
{
    family = AF_UNSPEC;

    if (ace.attrs_count > MAX_ACL_ATTRS) {
        return SAI_STATUS_INVALID_PARAMETER;
    }

    for (uint32_t i = 0; i < ace.attrs_count; i++) {
        const auto &attr = ace.attrs[i];
        int field_family = AF_UNSPEC;

        switch (attr.id) {
        case SAI_ACL_ENTRY_ATTR_FIELD_SRC_IP:
        case SAI_ACL_ENTRY_ATTR_FIELD_DST_IP:
        case SAI_ACL_ENTRY_ATTR_FIELD_INNER_SRC_IP:
        case SAI_ACL_ENTRY_ATTR_FIELD_INNER_DST_IP:
        case SAI_ACL_ENTRY_ATTR_FIELD_ICMP_TYPE:
        case SAI_ACL_ENTRY_ATTR_FIELD_ICMP_CODE:
            if (attr.value.aclfield.enable) {
                field_family = AF_INET;
            }
            break;

        case SAI_ACL_ENTRY_ATTR_FIELD_SRC_IPV6:
        case SAI_ACL_ENTRY_ATTR_FIELD_DST_IPV6:
        case SAI_ACL_ENTRY_ATTR_FIELD_INNER_SRC_IPV6:
        case SAI_ACL_ENTRY_ATTR_FIELD_INNER_DST_IPV6:
        case SAI_ACL_ENTRY_ATTR_FIELD_IPV6_NEXT_HEADER:
        case SAI_ACL_ENTRY_ATTR_FIELD_ICMPV6_TYPE:
        case SAI_ACL_ENTRY_ATTR_FIELD_ICMPV6_CODE:
            if (attr.value.aclfield.enable) {
                field_family = AF_INET6;
            }
            break;

        case SAI_ACL_ENTRY_ATTR_FIELD_ACL_IP_TYPE:
            if (attr.value.aclfield.enable) {
                if (attr.value.aclfield.data.s32 == SAI_ACL_IP_TYPE_IPV4ANY) {
                    field_family = AF_INET;
                } else if (attr.value.aclfield.data.s32 == SAI_ACL_IP_TYPE_IPV6ANY) {
                    field_family = AF_INET6;
                }
            }
            break;

        case SAI_ACL_ENTRY_ATTR_FIELD_ETHER_TYPE:
            if (attr.value.aclfield.enable && attr.value.aclfield.mask.u16 == 0xffff) {
                if (attr.value.aclfield.data.u16 == 0x0800) {
                    field_family = AF_INET;
                } else if (attr.value.aclfield.data.u16 == 0x86dd) {
                    field_family = AF_INET6;
                }
            }
            break;

        default:
            break;
        }

        if (field_family != AF_UNSPEC) {
            if (family != AF_UNSPEC && family != field_family) {
                return SAI_STATUS_INVALID_PARAMETER;
            }
            family = field_family;
        }
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t saivs::expandAclRuleVariants(
    const vpp_acl_rule_t &rule,
    int family,
    bool table_has_v4,
    bool table_has_v6,
    bool has_ports,
    std::vector<vpp_acl_rule_t> &variants)
{
    variants.clear();

    if (family != AF_UNSPEC && family != AF_INET && family != AF_INET6) {
        return SAI_STATUS_INVALID_PARAMETER;
    }

    for (const auto *prefix : {&rule.src_prefix, &rule.dst_prefix}) {
        if (vpp_ip_addr_family_is_set(prefix)) {
            if (family != AF_UNSPEC && family != prefix->sa_family) {
                return SAI_STATUS_INVALID_PARAMETER;
            }
            family = prefix->sa_family;
        }
    }

    if (has_ports && rule.proto != 0 && rule.proto != IPPROTO_TCP && rule.proto != IPPROTO_UDP) {
        return SAI_STATUS_INVALID_PARAMETER;
    }

    // A table without family-specific matches (e.g. VLAN-only) admits both IP
    // families. Declared L3/MIRROR and L3V6/MIRRORV6 tables remain single-family.
    bool want_v4 = family == AF_INET ||
        (family == AF_UNSPEC && (table_has_v4 || !table_has_v6));
    bool want_v6 = family == AF_INET6 ||
        (family == AF_UNSPEC && (table_has_v6 || !table_has_v4));

    for (int variant_family : {AF_INET, AF_INET6}) {
        if ((variant_family == AF_INET && !want_v4) ||
            (variant_family == AF_INET6 && !want_v6)) {
            continue;
        }

        vpp_acl_rule_t variant = rule;
        sai_status_t status = setAclRuleIpFamilyWildcard(variant, variant_family);
        if (status != SAI_STATUS_SUCCESS) {
            variants.clear();
            return status;
        }

        if (has_ports && variant.proto == 0) {
            variant.proto = IPPROTO_UDP;
            variants.push_back(variant);
            variant.proto = IPPROTO_TCP;
            variants.push_back(variant);
        } else {
            variants.push_back(variant);
        }
    }

    return SAI_STATUS_SUCCESS;
}
