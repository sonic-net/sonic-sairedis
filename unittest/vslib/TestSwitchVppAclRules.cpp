#include "sai.h"
#include "vpp/SwitchVppAcl.h"

#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <algorithm>
#include <cstring>
#include <set>
#include <utility>

using namespace saivs;

namespace
{
    using PacketClasses = std::set<std::pair<int, int>>;

    PacketClasses packetClasses(const std::vector<vpp_acl_rule_t> &rules)
    {
        PacketClasses result;
        for (const auto &rule : rules) {
            EXPECT_EQ(rule.src_prefix.sa_family, rule.dst_prefix.sa_family);
            result.emplace(rule.src_prefix.sa_family, rule.proto);
        }
        // Multiple rules for one packet class could count or mirror twice.
        EXPECT_EQ(result.size(), rules.size());
        return result;
    }

    sai_attribute_t enabledField(sai_acl_entry_attr_t id)
    {
        sai_attribute_t attr = {};
        attr.id = id;
        attr.value.aclfield.enable = true;
        return attr;
    }
}

TEST(SwitchVppAclRules, CustomDestinationOnlyIpv6KeepsPrefixAndNormalizesWildcard)
{
    acl_tbl_entries_t ace = {};
    ace.attrs[0] = enabledField(SAI_ACL_ENTRY_ATTR_FIELD_DST_IPV6);
    ace.attrs[1] = enabledField(SAI_ACL_ENTRY_ATTR_FIELD_IPV6_NEXT_HEADER);
    ace.attrs[1].value.aclfield.data.u8 = IPPROTO_TCP;
    ace.attrs[1].value.aclfield.mask.u8 = 0xff;
    ace.attrs_count = 2;
    int family = AF_UNSPEC;
    ASSERT_EQ(SAI_STATUS_SUCCESS, getAclRuleIpFamily(ace, family));

    vpp_acl_rule_t rule = {};
    rule.proto = IPPROTO_TCP;
    rule.action = VPP_ACL_ACTION_API_PERMIT;
    rule.dst_prefix.sa_family = AF_INET6;
    ASSERT_EQ(1, inet_pton(AF_INET6, "103:23:2:1::1",
                          &rule.dst_prefix.addr.ip6.sin6_addr));
    memset(&rule.dst_prefix_mask.addr.ip6.sin6_addr, 0xff, sizeof(in6_addr));

    std::vector<vpp_acl_rule_t> variants;
    ASSERT_EQ(SAI_STATUS_SUCCESS, expandAclRuleVariants(rule, family, true, true, false, variants));
    EXPECT_EQ((PacketClasses{{AF_INET6, IPPROTO_TCP}}), packetClasses(variants));
    ASSERT_EQ(1u, variants.size());
    EXPECT_EQ(0, memcmp(&rule.dst_prefix, &variants[0].dst_prefix, sizeof(rule.dst_prefix)));
    EXPECT_EQ(0, memcmp(&rule.dst_prefix_mask, &variants[0].dst_prefix_mask,
                        sizeof(rule.dst_prefix_mask)));
    EXPECT_TRUE(IN6_IS_ADDR_UNSPECIFIED(&variants[0].src_prefix.addr.ip6.sin6_addr));
    EXPECT_TRUE(IN6_IS_ADDR_UNSPECIFIED(&variants[0].src_prefix_mask.addr.ip6.sin6_addr));
}

TEST(SwitchVppAclRules, SourceOnlyIpv6KeepsPrefixAndNormalizesWildcard)
{
    vpp_acl_rule_t rule = {};
    rule.src_prefix.sa_family = AF_INET6;
    ASSERT_EQ(1, inet_pton(AF_INET6, "2001:db8::1", &rule.src_prefix.addr.ip6.sin6_addr));
    memset(&rule.src_prefix_mask.addr.ip6.sin6_addr, 0xff, sizeof(in6_addr));
    std::vector<vpp_acl_rule_t> variants;
    ASSERT_EQ(SAI_STATUS_SUCCESS, expandAclRuleVariants(rule, AF_UNSPEC, false, false, false, variants));
    EXPECT_EQ((PacketClasses{{AF_INET6, 0}}), packetClasses(variants));
    ASSERT_EQ(1u, variants.size());
    EXPECT_EQ(0, memcmp(&rule.src_prefix, &variants[0].src_prefix, sizeof(rule.src_prefix)));
    EXPECT_TRUE(IN6_IS_ADDR_UNSPECIFIED(&variants[0].dst_prefix.addr.ip6.sin6_addr));
}

TEST(SwitchVppAclRules, IpTypeAfterExplicitPrefixDoesNotEraseTheMatch)
{
    for (int family : {AF_INET, AF_INET6}) {
        for (bool source_prefix : {false, true}) {
            vpp_acl_rule_t rule = {};
            auto &prefix = source_prefix ? rule.src_prefix : rule.dst_prefix;
            auto &mask = source_prefix ? rule.src_prefix_mask : rule.dst_prefix_mask;
            prefix.sa_family = family;
            mask.sa_family = family;
            if (family == AF_INET6) {
                ASSERT_EQ(1, inet_pton(AF_INET6, "2001:db8::1", &prefix.addr.ip6.sin6_addr));
                memset(&mask.addr.ip6.sin6_addr, 0xff, sizeof(in6_addr));
            } else {
                ASSERT_EQ(1, inet_pton(AF_INET, "192.0.2.1", &prefix.addr.ip4.sin_addr));
                mask.addr.ip4.sin_addr.s_addr = 0xffffffff;
            }
            const auto explicit_prefix = prefix;
            const auto explicit_mask = mask;
            // The IPV4ANY/IPV6ANY translator uses this helper regardless of
            // whether IP_TYPE comes before or after a prefix attribute.
            ASSERT_EQ(SAI_STATUS_SUCCESS, setAclRuleIpFamilyWildcard(rule, family));
            EXPECT_EQ(0, memcmp(&explicit_prefix, &prefix, sizeof(prefix)));
            EXPECT_EQ(0, memcmp(&explicit_mask, &mask, sizeof(mask)));
            EXPECT_EQ(family, rule.src_prefix.sa_family);
            EXPECT_EQ(family, rule.dst_prefix.sa_family);
        }
    }
}

TEST(SwitchVppAclRules, FamilyNeutralVlanRuleMatchesBothIpVersions)
{
    vpp_acl_rule_t rule = {};
    sai_attribute_value_t value = {};
    value.aclfield.enable = true;
    value.aclfield.data.u16 = 100;
    value.aclfield.mask.u16 = 0x0fff;
    ASSERT_EQ(SAI_STATUS_SUCCESS, acl_rule_set_outer_vlan(&value, &rule));
    std::vector<vpp_acl_rule_t> variants;
    ASSERT_EQ(SAI_STATUS_SUCCESS, expandAclRuleVariants(rule, AF_UNSPEC, false, false, false, variants));
    EXPECT_EQ((PacketClasses{{AF_INET, 0}, {AF_INET6, 0}}), packetClasses(variants));
    for (const auto &variant : variants) {
        EXPECT_EQ(100, variant.outer_vlan_id);
        EXPECT_EQ(0x0fff, variant.outer_vlan_id_mask);
        EXPECT_EQ(VPP_ACL_ACTION_API_DENY, variant.action);
    }
}

TEST(SwitchVppAclRules, MixedTablePortRuleMatchesTcpAndUdpInBothIpVersions)
{
    vpp_acl_rule_t rule = {};
    rule.action = VPP_ACL_ACTION_API_PERMIT;
    rule.srcport_or_icmptype_first = 8081;
    rule.srcport_or_icmptype_last = 8090;
    rule.dstport_or_icmpcode_first = 8080;
    rule.dstport_or_icmpcode_last = 8080;
    std::vector<vpp_acl_rule_t> variants;
    ASSERT_EQ(SAI_STATUS_SUCCESS, expandAclRuleVariants(rule, AF_UNSPEC, true, true, true, variants));
    EXPECT_EQ((PacketClasses{{AF_INET, IPPROTO_TCP}, {AF_INET, IPPROTO_UDP},
                            {AF_INET6, IPPROTO_TCP}, {AF_INET6, IPPROTO_UDP}}),
              packetClasses(variants));
    for (const auto &variant : variants) {
        EXPECT_EQ(8081, variant.srcport_or_icmptype_first);
        EXPECT_EQ(8090, variant.srcport_or_icmptype_last);
        EXPECT_EQ(8080, variant.dstport_or_icmpcode_first);
        EXPECT_EQ(8080, variant.dstport_or_icmpcode_last);
        EXPECT_EQ(VPP_ACL_ACTION_API_PERMIT, variant.action);
    }
    EXPECT_EQ(0, rule.proto);
    EXPECT_EQ(AF_UNSPEC, rule.src_prefix.sa_family);
}

TEST(SwitchVppAclRules, TableFamilyAndExplicitProtocolRestrictPacketClasses)
{
    struct Scenario {
        bool table_v4;
        bool table_v6;
        int proto;
        bool has_ports;
        PacketClasses expected;
    };
    const Scenario scenarios[] = {
        {true, false, 0, false, {{AF_INET, 0}}},
        {false, true, 0, false, {{AF_INET6, 0}}},
        {true, true, IPPROTO_TCP, true, {{AF_INET, IPPROTO_TCP}, {AF_INET6, IPPROTO_TCP}}},
        {false, true, 0, true, {{AF_INET6, IPPROTO_TCP}, {AF_INET6, IPPROTO_UDP}}},
    };
    for (const auto &scenario : scenarios) {
        vpp_acl_rule_t rule = {};
        rule.action = VPP_ACL_ACTION_PERMIT_MIRROR;
        rule.mirror_action = 17;
        rule.proto = scenario.proto;
        std::vector<vpp_acl_rule_t> variants;
        ASSERT_EQ(SAI_STATUS_SUCCESS, expandAclRuleVariants(rule, AF_UNSPEC, scenario.table_v4,
                                                          scenario.table_v6, scenario.has_ports, variants));
        EXPECT_EQ(scenario.expected, packetClasses(variants));
        for (const auto &variant : variants) {
            EXPECT_EQ(17u, variant.mirror_action);
            EXPECT_EQ(VPP_ACL_ACTION_PERMIT_MIRROR, variant.action);
        }
    }
}

TEST(SwitchVppAclRules, NextHeaderWithoutPrefixImpliesIpv6)
{
    acl_tbl_entries_t ace = {};
    ace.attrs[0] = enabledField(SAI_ACL_ENTRY_ATTR_FIELD_IPV6_NEXT_HEADER);
    ace.attrs_count = 1;
    int family = AF_UNSPEC;
    ASSERT_EQ(SAI_STATUS_SUCCESS, getAclRuleIpFamily(ace, family));
    vpp_acl_rule_t rule = {};
    rule.proto = IPPROTO_UDP;
    std::vector<vpp_acl_rule_t> variants;
    ASSERT_EQ(SAI_STATUS_SUCCESS, expandAclRuleVariants(rule, family, true, true, false, variants));
    EXPECT_EQ((PacketClasses{{AF_INET6, IPPROTO_UDP}}), packetClasses(variants));
}

TEST(SwitchVppAclRules, GenericIpTypesRemainFamilyNeutral)
{
    for (auto type : {SAI_ACL_IP_TYPE_ANY, SAI_ACL_IP_TYPE_IP}) {
        acl_tbl_entries_t ace = {};
        ace.attrs[0] = enabledField(SAI_ACL_ENTRY_ATTR_FIELD_ACL_IP_TYPE);
        ace.attrs[0].value.aclfield.data.s32 = type;
        ace.attrs_count = 1;
        int family = AF_INET;
        ASSERT_EQ(SAI_STATUS_SUCCESS, getAclRuleIpFamily(ace, family));
        EXPECT_EQ(AF_UNSPEC, family);
        std::vector<vpp_acl_rule_t> variants;
        ASSERT_EQ(SAI_STATUS_SUCCESS, expandAclRuleVariants({}, family, false, false, false, variants));
        EXPECT_EQ((PacketClasses{{AF_INET, 0}, {AF_INET6, 0}}), packetClasses(variants));
    }
}

TEST(SwitchVppAclRules, FamilyConflictsRejectRegardlessOfAttributeOrder)
{
    const sai_acl_entry_attr_t ipv6_fields[] = {
        SAI_ACL_ENTRY_ATTR_FIELD_DST_IPV6,
        SAI_ACL_ENTRY_ATTR_FIELD_SRC_IPV6,
        SAI_ACL_ENTRY_ATTR_FIELD_IPV6_NEXT_HEADER,
        SAI_ACL_ENTRY_ATTR_FIELD_ICMPV6_TYPE,
    };
    for (auto field : ipv6_fields) {
        acl_tbl_entries_t ace = {};
        ace.attrs[0] = enabledField(SAI_ACL_ENTRY_ATTR_FIELD_ACL_IP_TYPE);
        ace.attrs[0].value.aclfield.data.s32 = SAI_ACL_IP_TYPE_IPV4ANY;
        ace.attrs[1] = enabledField(field);
        ace.attrs_count = 2;
        int family;
        EXPECT_EQ(SAI_STATUS_INVALID_PARAMETER, getAclRuleIpFamily(ace, family));
        std::swap(ace.attrs[0], ace.attrs[1]);
        EXPECT_EQ(SAI_STATUS_INVALID_PARAMETER, getAclRuleIpFamily(ace, family));
        ace.attrs[0].value.aclfield.enable = false;
        EXPECT_EQ(SAI_STATUS_SUCCESS, getAclRuleIpFamily(ace, family));
        EXPECT_EQ(AF_INET, family);
    }
}

TEST(SwitchVppAclRules, PrefixAndPortProtocolConflictsDoNotEmitRules)
{
    vpp_acl_rule_t rule = {};
    rule.src_prefix.sa_family = AF_INET;
    rule.dst_prefix.sa_family = AF_INET6;
    std::vector<vpp_acl_rule_t> variants(1);
    EXPECT_EQ(SAI_STATUS_INVALID_PARAMETER,
              expandAclRuleVariants(rule, AF_UNSPEC, true, true, false, variants));
    EXPECT_TRUE(variants.empty());

    rule = {};
    rule.proto = IPPROTO_ICMPV6;
    EXPECT_EQ(SAI_STATUS_INVALID_PARAMETER,
              expandAclRuleVariants(rule, AF_INET6, true, true, true, variants));
    EXPECT_TRUE(variants.empty());
}

TEST(SwitchVppAclRules, OuterVlanMasksAndDisabledMatch)
{
    sai_attribute_value_t value = {};
    value.aclfield.enable = true;
    value.aclfield.data.u16 = 0x0123;
    value.aclfield.mask.u16 = 0x0ff0;
    vpp_acl_rule_t rule = {};
    ASSERT_EQ(SAI_STATUS_SUCCESS, acl_rule_set_outer_vlan(&value, &rule));
    EXPECT_EQ(0x0120, rule.outer_vlan_id);
    EXPECT_EQ(0x0ff0, rule.outer_vlan_id_mask);

    value.aclfield.enable = false;
    ASSERT_EQ(SAI_STATUS_SUCCESS, acl_rule_set_outer_vlan(&value, &rule));
    EXPECT_EQ(0, rule.outer_vlan_id);
    EXPECT_EQ(0, rule.outer_vlan_id_mask);

    value.aclfield.enable = true;
    value.aclfield.data.u16 = 0;
    value.aclfield.mask.u16 = 0x0fff;
    EXPECT_EQ(SAI_STATUS_SUCCESS, acl_rule_set_outer_vlan(&value, &rule));
    EXPECT_EQ(0, rule.outer_vlan_id);
    EXPECT_EQ(0x0fff, rule.outer_vlan_id_mask);

    value.aclfield.data.u16 = 0x1000;
    EXPECT_EQ(SAI_STATUS_INVALID_PARAMETER, acl_rule_set_outer_vlan(&value, &rule));
    value.aclfield.data.u16 = 100;
    value.aclfield.mask.u16 = 0x1000;
    EXPECT_EQ(SAI_STATUS_INVALID_PARAMETER, acl_rule_set_outer_vlan(&value, &rule));
}
