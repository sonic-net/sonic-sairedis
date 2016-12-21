#include "sai_meta.h"

// METADATA for SAI_OBJECT_TYPE_ACL_TABLE

#define DEF_ACL_TABLE_FIELD(x)\
    {\
        .objecttype             = SAI_OBJECT_TYPE_ACL_TABLE,\
        .attrid                 = SAI_ACL_TABLE_ATTR_FIELD_ ## x,\
        .attridname             = "SAI_ACL_TABLE_ATTR_FIELD_" #x,\
        .serializationtype      = SAI_SERIALIZATION_TYPE_BOOL,\
        .flags                  = SAI_ATTR_FLAGS_CREATE_ONLY,\
        .allowedobjecttypes     = { },\
        .allownullobjectid      = false,\
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,\
        .defaultvalue           = { .booldata = false },\
        .enumtypestr            = NULL,\
        .enumallowedvalues      = { },\
        .enummetadata           = NULL,\
        .conditions             = { },\
    },

DEFINE_ENUM_VALUES(sai_acl_bind_point_t)
{
    SAI_ACL_BIND_POINT_PORT,
    SAI_ACL_BIND_POINT_LAG,
    SAI_ACL_BIND_POINT_VLAN,
    SAI_ACL_BIND_POINT_ROUTER_INTF,
    SAI_ACL_BIND_POINT_SWITCH
};

DEFINE_ENUM_VALUES(sai_acl_stage_t)
{
    SAI_ACL_STAGE_INGRESS,
    SAI_ACL_STAGE_EGRESS,
    SAI_ACL_SUBSTAGE_INGRESS_PRE_L2,
    SAI_ACL_SUBSTAGE_INGRESS_POST_L3
};

DEFINE_ENUM_VALUES(sai_acl_range_type_t)
{
    SAI_ACL_RANGE_L4_SRC_PORT_RANGE,
    SAI_ACL_RANGE_L4_DST_PORT_RANGE,
    SAI_ACL_RANGE_OUTER_VLAN,
    SAI_ACL_RANGE_INNER_VLAN,
    SAI_ACL_RANGE_PACKET_LENGTH
};

DEFINE_ENUM_VALUES(sai_packet_color_t)
{
    SAI_PACKET_COLOR_GREEN,
    SAI_PACKET_COLOR_YELLOW,
    SAI_PACKET_COLOR_RED
};

DEFINE_ENUM_VALUES(sai_acl_ip_type_t)
{
    SAI_ACL_IP_TYPE_ANY,
    SAI_ACL_IP_TYPE_IP,
    SAI_ACL_IP_TYPE_NON_IP,
    SAI_ACL_IP_TYPE_IPv4ANY,
    SAI_ACL_IP_TYPE_NON_IPv4,
    SAI_ACL_IP_TYPE_IPv6ANY,
    SAI_ACL_IP_TYPE_NON_IPv6,
    SAI_ACL_IP_TYPE_ARP,
    SAI_ACL_IP_TYPE_ARP_REQUEST,
    SAI_ACL_IP_TYPE_ARP_REPLY
};

DEFINE_ENUM_VALUES(sai_acl_ip_frag_t)
{
    SAI_ACL_IP_FRAG_ANY,
    SAI_ACL_IP_FRAG_NON_FRAG,
    SAI_ACL_IP_FRAG_NON_FRAG_OR_HEAD,
    SAI_ACL_IP_FRAG_HEAD,
    SAI_ACL_IP_FRAG_NON_HEAD
};

DEFINE_ENUM_VALUES(sai_packet_vlan_t)
{
    SAI_PACKET_VLAN_UNTAG,
    SAI_PACKET_VLAN_SINGLE_OUTER_TAG,
    SAI_PACKET_VLAN_DOUBLE_TAG
};

const char metadata_sai_packet_color_t_enum_name[] = "sai_packet_color_t";
const sai_packet_color_t metadata_sai_packet_color_t_enum_values[] = {
    SAI_PACKET_COLOR_GREEN,
    SAI_PACKET_COLOR_YELLOW,
    SAI_PACKET_COLOR_RED,
};
const char* metadata_sai_packet_color_t_enum_values_names[] = {
    "SAI_PACKET_COLOR_GREEN",
    "SAI_PACKET_COLOR_YELLOW",
    "SAI_PACKET_COLOR_RED",
    NULL
};
const char* metadata_sai_packet_color_t_enum_values_short_names[] = {
    "GREEN",
    "YELLOW",
    "RED",
    NULL
};
const size_t metadata_sai_packet_color_t_enum_values_count = 3;
DEFINE_ENUM_METADATA(sai_packet_color_t, 3);

const char metadata_sai_acl_bind_point_t_enum_name[] = "sai_acl_bind_point_t";
const sai_acl_bind_point_t metadata_sai_acl_bind_point_t_enum_values[] = {
    SAI_ACL_BIND_POINT_PORT,
    SAI_ACL_BIND_POINT_LAG,
    SAI_ACL_BIND_POINT_VLAN,
    SAI_ACL_BIND_POINT_ROUTER_INTF,
    SAI_ACL_BIND_POINT_SWITCH
};
const char* metadata_sai_acl_bind_point_t_enum_values_names[] = {
    "SAI_ACL_BIND_POINT_PORT",
    "SAI_ACL_BIND_POINT_LAG",
    "SAI_ACL_BIND_POINT_VLAN",
    "SAI_ACL_BIND_POINT_ROUTER_INTF",
    "SAI_ACL_BIND_POINT_SWITCH"
};
const char* metadata_sai_acl_bind_point_t_enum_values_short_names[] = {
    "PORT",
    "LAG",
    "VLAN",
    "ROUTER_INTF",
    "SWITCH"
};
const size_t metadata_sai_acl_bind_point_t_enum_values_count = 4;
DEFINE_ENUM_METADATA(sai_acl_bind_point_t, 4);

const char metadata_sai_acl_stage_t_enum_name[] = "sai_acl_stage_t";
const sai_acl_stage_t metadata_sai_acl_stage_t_enum_values[] = {
    SAI_ACL_STAGE_INGRESS,
    SAI_ACL_STAGE_EGRESS,
    SAI_ACL_SUBSTAGE_INGRESS_PRE_L2,
    SAI_ACL_SUBSTAGE_INGRESS_POST_L3,
};
const char* metadata_sai_acl_stage_t_enum_values_names[] = {
    "SAI_ACL_STAGE_INGRESS",
    "SAI_ACL_STAGE_EGRESS",
    "SAI_ACL_STAGE_SUBSTAGE_INGRESS_PRE_L2",
    "SAI_ACL_STAGE_SUBSTAGE_INGRESS_POST_L3",
    NULL
};
const char* metadata_sai_acl_stage_t_enum_values_short_names[] = {
    "INGRESS",
    "EGRESS",
    "SUBSTAGE_INGRESS_PRE_L2",
    "SUBSTAGE_INGRESS_POST_L3",
    NULL
};
const size_t metadata_sai_acl_stage_t_enum_values_count = 4;
DEFINE_ENUM_METADATA(sai_acl_stage_t, 4);

const char metadata_sai_packet_action_t_enum_name[] = "sai_packet_action_t";
const sai_packet_action_t metadata_sai_packet_action_t_enum_values[] = {
    SAI_PACKET_ACTION_DROP,
    SAI_PACKET_ACTION_FORWARD,
    SAI_PACKET_ACTION_COPY,
    SAI_PACKET_ACTION_COPY_CANCEL,
    SAI_PACKET_ACTION_TRAP,
    SAI_PACKET_ACTION_LOG,
    SAI_PACKET_ACTION_DENY,
    SAI_PACKET_ACTION_TRANSIT,
};
const char* metadata_sai_packet_action_t_enum_values_names[] = {
    "SAI_PACKET_ACTION_DROP",
    "SAI_PACKET_ACTION_FORWARD",
    "SAI_PACKET_ACTION_COPY",
    "SAI_PACKET_ACTION_COPY_CANCEL",
    "SAI_PACKET_ACTION_TRAP",
    "SAI_PACKET_ACTION_LOG",
    "SAI_PACKET_ACTION_DENY",
    "SAI_PACKET_ACTION_TRANSIT",
    NULL
};
const char* metadata_sai_packet_action_t_enum_values_short_names[] = {
    "DROP",
    "FORWARD",
    "COPY",
    "COPY_CANCEL",
    "TRAP",
    "LOG",
    "DENY",
    "TRANSIT",
    NULL
};
const size_t metadata_sai_packet_action_t_enum_values_count = 8;
DEFINE_ENUM_METADATA(sai_packet_action_t, 8);

const char metadata_sai_packet_vlan_t_enum_name[] = "sai_packet_vlan_t";
const sai_packet_vlan_t metadata_sai_packet_vlan_t_enum_values[] = {
    SAI_PACKET_VLAN_UNTAG,
    SAI_PACKET_VLAN_SINGLE_OUTER_TAG,
    SAI_PACKET_VLAN_DOUBLE_TAG,
};
const char* metadata_sai_packet_vlan_t_enum_values_names[] = {
    "SAI_PACKET_VLAN_UNTAG",
    "SAI_PACKET_VLAN_SINGLE_OUTER_TAG",
    "SAI_PACKET_VLAN_DOUBLE_TAG",
    NULL
};
const char* metadata_sai_packet_vlan_t_enum_values_short_names[] = {
    "UNTAG",
    "SINGLE_OUTER_TAG",
    "DOUBLE_TAG",
    NULL
};
const size_t metadata_sai_packet_vlan_t_enum_values_count = 3;
DEFINE_ENUM_METADATA(sai_packet_vlan_t, 3);

const char metadata_sai_acl_ip_frag_t_enum_name[] = "sai_acl_ip_frag_t";
const sai_acl_ip_frag_t metadata_sai_acl_ip_frag_t_enum_values[] = {
    SAI_ACL_IP_FRAG_ANY,
    SAI_ACL_IP_FRAG_NON_FRAG,
    SAI_ACL_IP_FRAG_NON_FRAG_OR_HEAD,
    SAI_ACL_IP_FRAG_HEAD,
    SAI_ACL_IP_FRAG_NON_HEAD,
};
const char* metadata_sai_acl_ip_frag_t_enum_values_names[] = {
    "SAI_ACL_IP_FRAG_ANY",
    "SAI_ACL_IP_FRAG_NON_FRAG",
    "SAI_ACL_IP_FRAG_NON_FRAG_OR_HEAD",
    "SAI_ACL_IP_FRAG_HEAD",
    "SAI_ACL_IP_FRAG_NON_HEAD",
    NULL
};
const char* metadata_sai_acl_ip_frag_t_enum_values_short_names[] = {
    "ANY",
    "NON_FRAG",
    "NON_FRAG_OR_HEAD",
    "HEAD",
    "NON_HEAD",
    NULL
};
const size_t metadata_sai_acl_ip_frag_t_enum_values_count = 5;
DEFINE_ENUM_METADATA(sai_acl_ip_frag_t, 5);

const char metadata_sai_acl_ip_type_t_enum_name[] = "sai_acl_ip_type_t";
const sai_acl_ip_type_t metadata_sai_acl_ip_type_t_enum_values[] = {
    SAI_ACL_IP_TYPE_ANY,
    SAI_ACL_IP_TYPE_IP,
    SAI_ACL_IP_TYPE_NON_IP,
    SAI_ACL_IP_TYPE_IPv4ANY,
    SAI_ACL_IP_TYPE_NON_IPv4,
    SAI_ACL_IP_TYPE_IPv6ANY,
    SAI_ACL_IP_TYPE_NON_IPv6,
    SAI_ACL_IP_TYPE_ARP,
    SAI_ACL_IP_TYPE_ARP_REQUEST,
    SAI_ACL_IP_TYPE_ARP_REPLY,
};
const char* metadata_sai_acl_ip_type_t_enum_values_names[] = {
    "SAI_ACL_IP_TYPE_ANY",
    "SAI_ACL_IP_TYPE_IP",
    "SAI_ACL_IP_TYPE_NON_IP",
    "SAI_ACL_IP_TYPE_IPv4ANY",
    "SAI_ACL_IP_TYPE_NON_IPv4",
    "SAI_ACL_IP_TYPE_IPv6ANY",
    "SAI_ACL_IP_TYPE_NON_IPv6",
    "SAI_ACL_IP_TYPE_ARP",
    "SAI_ACL_IP_TYPE_ARP_REQUEST",
    "SAI_ACL_IP_TYPE_ARP_REPLY",
    NULL
};
const char* metadata_sai_acl_ip_type_t_enum_values_short_names[] = {
    "ANY",
    "IP",
    "NON_IP",
    "IPv4ANY",
    "NON_IPv4",
    "IPv6ANY",
    "NON_IPv6",
    "ARP",
    "ARP_REQUEST",
    "ARP_REPLY",
    NULL
};
const size_t metadata_sai_acl_ip_type_t_enum_values_count = 10;
DEFINE_ENUM_METADATA(sai_acl_ip_type_t, 10);

const char metadata_sai_acl_range_type_t_enum_name[] = "sai_acl_range_type_t";
const sai_acl_range_type_t metadata_sai_acl_range_type_t_enum_values[] = {
    SAI_ACL_RANGE_L4_SRC_PORT_RANGE,
    SAI_ACL_RANGE_L4_DST_PORT_RANGE,
    SAI_ACL_RANGE_OUTER_VLAN,
    SAI_ACL_RANGE_INNER_VLAN,
    SAI_ACL_RANGE_PACKET_LENGTH,
};
const char* metadata_sai_acl_range_type_t_enum_values_names[] = {
    "SAI_ACL_RANGE_TYPE_L4_SRC_PORT_RANGE",
    "SAI_ACL_RANGE_TYPE_L4_DST_PORT_RANGE",
    "SAI_ACL_RANGE_TYPE_OUTER_VLAN",
    "SAI_ACL_RANGE_TYPE_INNER_VLAN",
    "SAI_ACL_RANGE_TYPE_PACKET_LENGTH",
    NULL
};
const char* metadata_sai_acl_range_type_t_enum_values_short_names[] = {
    "L4_SRC_PORT_RANGE",
    "L4_DST_PORT_RANGE",
    "OUTER_VLAN",
    "INNER_VLAN",
    "PACKET_LENGTH",
    NULL
};
const size_t metadata_sai_acl_range_type_t_enum_values_count = 5;
DEFINE_ENUM_METADATA(sai_acl_range_type_t, 5);

const sai_attr_metadata_t sai_acl_table_attr_metadata[] = {

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_TABLE,
        .attrid                 = SAI_ACL_TABLE_ATTR_BIND_POINT,
        .attridname             = "SAI_ACL_TABLE_ATTR_BIND_POINT",
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = StringifyEnum ( sai_acl_bind_point_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_acl_bind_point_t ),
        .enummetadata           = &metadata_enum_sai_acl_bind_point_t,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_TABLE,
        .attrid                 = SAI_ACL_TABLE_ATTR_STAGE,
        .attridname             = "SAI_ACL_TABLE_ATTR_STAGE",
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = StringifyEnum ( sai_acl_stage_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_acl_stage_t ),
        .enummetadata           = &metadata_enum_sai_acl_stage_t,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_TABLE,
        .attrid                 = SAI_ACL_TABLE_ATTR_PRIORITY,
        .attridname             = "SAI_ACL_TABLE_ATTR_PRIORITY",
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT32,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_ATTR_RANGE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },

        // Value in range
        // SAI_SWITCH_ATTR_ACL_TABLE_MINIMUM_PRIORITY
        // SAI_SWITCH_ATTR_ACL_TABLE_MAXIMUM_PRIORITY
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_TABLE,
        .attrid                 = SAI_ACL_TABLE_ATTR_SIZE,
        .attridname             = "SAI_ACL_TABLE_ATTR_SIZE",
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .u32 = 0 },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },

        // TODO this attribute is special, since it can change dynamically, dynamic
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_TABLE,
        .attrid                 = SAI_ACL_TABLE_ATTR_GROUP_ID,
        .attridname             = "SAI_ACL_TABLE_ATTR_GROUP_ID",
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_ACL_TABLE_GROUP },  // TODO there is no API to create this
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .oid = SAI_NULL_OBJECT_ID },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },

        // TODO group ID is special, since when not specified it's created automatically
        // by switch and user can obtain it via GET api and group tables.
        // This behaviour should be changed so no object would be created internally.
        // It will be tricky to track this object usage.
    },

    // ACL TABLE FIELDS

    // NOTE: At least one field must be specified this is extra condition on create function

    DEF_ACL_TABLE_FIELD ( SRC_IPv6 )
    DEF_ACL_TABLE_FIELD ( DST_IPv6 )
    DEF_ACL_TABLE_FIELD ( SRC_MAC )
    DEF_ACL_TABLE_FIELD ( DST_MAC )
    DEF_ACL_TABLE_FIELD ( SRC_IP )
    DEF_ACL_TABLE_FIELD ( DST_IP )
    DEF_ACL_TABLE_FIELD ( IN_PORTS )
    DEF_ACL_TABLE_FIELD ( OUT_PORTS )
    DEF_ACL_TABLE_FIELD ( IN_PORT )
    DEF_ACL_TABLE_FIELD ( OUT_PORT )
    DEF_ACL_TABLE_FIELD ( SRC_PORT )
    DEF_ACL_TABLE_FIELD ( OUTER_VLAN_ID )
    DEF_ACL_TABLE_FIELD ( OUTER_VLAN_PRI )
    DEF_ACL_TABLE_FIELD ( OUTER_VLAN_CFI )
    DEF_ACL_TABLE_FIELD ( INNER_VLAN_ID )
    DEF_ACL_TABLE_FIELD ( INNER_VLAN_PRI )
    DEF_ACL_TABLE_FIELD ( INNER_VLAN_CFI )
    DEF_ACL_TABLE_FIELD ( L4_SRC_PORT )
    DEF_ACL_TABLE_FIELD ( L4_DST_PORT )
    DEF_ACL_TABLE_FIELD ( ETHER_TYPE )
    DEF_ACL_TABLE_FIELD ( IP_PROTOCOL )
    DEF_ACL_TABLE_FIELD ( DSCP )
    DEF_ACL_TABLE_FIELD ( ECN )
    DEF_ACL_TABLE_FIELD ( TTL )
    DEF_ACL_TABLE_FIELD ( TOS )
    DEF_ACL_TABLE_FIELD ( IP_FLAGS )
    DEF_ACL_TABLE_FIELD ( TCP_FLAGS )
    DEF_ACL_TABLE_FIELD ( IP_TYPE )
    DEF_ACL_TABLE_FIELD ( IP_FRAG )
    DEF_ACL_TABLE_FIELD ( IPv6_FLOW_LABEL )
    DEF_ACL_TABLE_FIELD ( TC )
    DEF_ACL_TABLE_FIELD ( ICMP_TYPE )
    DEF_ACL_TABLE_FIELD ( ICMP_CODE )
    DEF_ACL_TABLE_FIELD ( VLAN_TAGS )
    DEF_ACL_TABLE_FIELD ( FDB_DST_USER_META )
    DEF_ACL_TABLE_FIELD ( ROUTE_DST_USER_META )
    DEF_ACL_TABLE_FIELD ( NEIGHBOR_DST_USER_META )
    DEF_ACL_TABLE_FIELD ( PORT_USER_META )
    DEF_ACL_TABLE_FIELD ( VLAN_USER_META )
    DEF_ACL_TABLE_FIELD ( ACL_USER_META )
    DEF_ACL_TABLE_FIELD ( FDB_NPU_META_DST_HIT )
    DEF_ACL_TABLE_FIELD ( NEIGHBOR_NPU_META_DST_HIT )
    DEF_ACL_TABLE_FIELD ( ROUTE_NPU_META_DST_HIT )

    // TODO SAI_ACL_TABLE_ATTR_USER_DEFINED_FIELD_GROUP_MIN .. SAI_ACL_TABLE_ATTR_USER_DEFINED_FIELD_GROUP_MAX

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_TABLE,
        .attrid                 = SAI_ACL_TABLE_ATTR_FIELD_RANGE,
        .attridname             = "SAI_ACL_TABLE_ATTR_FIELD_RANGE",
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .s32 = 0 }, // TODO need discussion since this attribute make no sense for some fields
        .enumtypestr            = StringifyEnum ( sai_acl_range_type_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_acl_range_type_t ),
        .enummetadata           = &metadata_enum_sai_acl_range_type_t,
        .conditions             = { },
    },

    //{
    //    .objecttype             = SAI_OBJECT_TYPE_ACL_TABLE,
    //    .attrid                 = SAI_ACL_TABLE_ATTR_ACTION_LIST
    //    .serializationtype      = SAI_SERIALIZATION_TYPE_INT32_LIST,
    //    .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY, // TODO if not mandatory, what is default?
    //    .allowedobjecttypes     = { },
    //    .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
    //    .allownullobjectid      = false,
    //    .defaultvalue           = { },
    //    .enumtypestr            = NULL,
    //    .enumallowedvalues      = { },
    //    .conditions             = { },

    //    // TODO this is special since if switch supports it, it may be mandatory to pass it
    //    // or it maybe mandatory to pass some fields and they may be const, or we can add
    //    // more dynamicaly, we need very special handling for this

    //    /** List of actions in sai_acl_table_action_list_t [sai_s32_list_t]
    //     * Based on the acl capability per stage obtained from the switch
    //     * attribute SAI_SWITCH_ATTR_ACL_CAPABILITY application should
    //     * pass the action list if its mandatory per stage.
    //     * If its not mandatory application can either pass the action list
    //     * or ignore it.
    //     */
    //},
};

const size_t sai_acl_table_attr_metadata_count = sizeof(sai_acl_table_attr_metadata)/sizeof(sai_attr_metadata_t);

// METADATA for SAI_OBJECT_TYPE_ACL_ENTRY

const sai_attr_metadata_t sai_acl_entry_attr_metadata[] = {

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_TABLE_ID,
        .attridname             = "SAI_ACL_ENTRY_ATTR_TABLE_ID",
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_ACL_TABLE },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_ADMIN_STATE,
        .attridname             = "SAI_ACL_ENTRY_ATTR_ADMIN_STATE",
        .serializationtype      = SAI_SERIALIZATION_TYPE_BOOL,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = {  },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .booldata = true },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_PRIORITY,
        .attridname             = "SAI_ACL_ENTRY_ATTR_PRIORITY",
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_ATTR_RANGE,
        .defaultvalue           = { .u32 = 0 },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },

        // valid range in SAI_SWITCH_ATTR_ACL_ENTRY_MINIMUM_PRIORITY .. SAI_SWITCH_ATTR_ACL_ENTRY_MAXIMUM_PRIORITY
    },

    // ACL ENTRY FIELDS

    // it is mandatory to pass at least one field this will require logic on creation time

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_FIELD_SRC_IPv6,
        .attridname             = "SAI_ACL_ENTRY_ATTR_FIELD_SRC_IPv6",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_IP6,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_FIELD_DST_IPv6,
        .attridname             = "SAI_ACL_ENTRY_ATTR_FIELD_DST_IPv6",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_IP6,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },


    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_FIELD_SRC_MAC,
        .attridname             = "SAI_ACL_ENTRY_ATTR_FIELD_SRC_MAC",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_MAC,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_FIELD_DST_MAC,
        .attridname             = "SAI_ACL_ENTRY_ATTR_FIELD_DST_MAC",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_MAC,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_FIELD_SRC_IP,
        .attridname             = "SAI_ACL_ENTRY_ATTR_FIELD_SRC_IP",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_IP4,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_FIELD_DST_IP,
        .attridname             = "SAI_ACL_ENTRY_ATTR_FIELD_DST_IP",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_IP4,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_FIELD_IN_PORTS,
        .attridname             = "SAI_ACL_ENTRY_ATTR_FIELD_IN_PORTS",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_OBJECT_LIST,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_PORT },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_EMPTY_LIST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_FIELD_OUT_PORTS,
        .attridname             = "SAI_ACL_ENTRY_ATTR_FIELD_OUT_PORTS",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_OBJECT_LIST,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_PORT },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_EMPTY_LIST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_FIELD_IN_PORT,
        .attridname             = "SAI_ACL_ENTRY_ATTR_FIELD_IN_PORT",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_PORT },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_FIELD_OUT_PORT,
        .attridname             = "SAI_ACL_ENTRY_ATTR_FIELD_OUT_PORT",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_PORT },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_FIELD_SRC_PORT,
        .attridname             = "SAI_ACL_ENTRY_ATTR_FIELD_SRC_PORT",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_PORT },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_FIELD_OUTER_VLAN_ID,
        .attridname             = "SAI_ACL_ENTRY_ATTR_FIELD_OUTER_VLAN_ID",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT16,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_FIELD_OUTER_VLAN_PRI,
        .attridname             = "SAI_ACL_ENTRY_ATTR_FIELD_OUTER_VLAN_PRI",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT8,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_FIELD_OUTER_VLAN_CFI,
        .attridname             = "SAI_ACL_ENTRY_ATTR_FIELD_OUTER_VLAN_CFI",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT8,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_FIELD_INNER_VLAN_ID,
        .attridname             = "SAI_ACL_ENTRY_ATTR_FIELD_INNER_VLAN_ID",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT16,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_FIELD_INNER_VLAN_PRI,
        .attridname             = "SAI_ACL_ENTRY_ATTR_FIELD_INNER_VLAN_PRI",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT8,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_FIELD_INNER_VLAN_CFI,
        .attridname             = "SAI_ACL_ENTRY_ATTR_FIELD_INNER_VLAN_CFI",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT8,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_FIELD_L4_SRC_PORT,
        .attridname             = "SAI_ACL_ENTRY_ATTR_FIELD_L4_SRC_PORT",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT16,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_FIELD_L4_DST_PORT,
        .attridname             = "SAI_ACL_ENTRY_ATTR_FIELD_L4_DST_PORT",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT16,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_FIELD_ETHER_TYPE,
        .attridname             = "SAI_ACL_ENTRY_ATTR_FIELD_ETHER_TYPE",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT16,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_FIELD_IP_PROTOCOL,
        .attridname             = "SAI_ACL_ENTRY_ATTR_FIELD_IP_PROTOCOL",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT8,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_FIELD_DSCP,
        .attridname             = "SAI_ACL_ENTRY_ATTR_FIELD_DSCP",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT8,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_FIELD_ECN,
        .attridname             = "SAI_ACL_ENTRY_ATTR_FIELD_ECN",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT8,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_FIELD_TTL,
        .attridname             = "SAI_ACL_ENTRY_ATTR_FIELD_TTL",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT8,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_FIELD_TOS,
        .attridname             = "SAI_ACL_ENTRY_ATTR_FIELD_TOS",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT8,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_FIELD_IP_FLAGS,
        .attridname             = "SAI_ACL_ENTRY_ATTR_FIELD_IP_FLAGS",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT8,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_FIELD_TCP_FLAGS,
        .attridname             = "SAI_ACL_ENTRY_ATTR_FIELD_TCP_FLAGS",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT8,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_FIELD_IP_TYPE,
        .attridname             = "SAI_ACL_ENTRY_ATTR_FIELD_IP_TYPE",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_INT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = StringifyEnum ( sai_acl_ip_type_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_acl_ip_type_t ),
        .enummetadata           = &metadata_enum_sai_acl_ip_type_t,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_FIELD_IP_FRAG,
        .attridname             = "SAI_ACL_ENTRY_ATTR_FIELD_IP_FRAG",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_INT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = StringifyEnum ( sai_acl_ip_frag_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_acl_ip_frag_t ),
        .enummetadata           = &metadata_enum_sai_acl_ip_frag_t,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_FIELD_IPv6_FLOW_LABEL,
        .attridname             = "SAI_ACL_ENTRY_ATTR_FIELD_IPv6_FLOW_LABEL",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_FIELD_TC,
        .attridname             = "SAI_ACL_ENTRY_ATTR_FIELD_TC",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT8,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_FIELD_ICMP_TYPE,
        .attridname             = "SAI_ACL_ENTRY_ATTR_FIELD_ICMP_TYPE",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT8,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_FIELD_ICMP_CODE,
        .attridname             = "SAI_ACL_ENTRY_ATTR_FIELD_ICMP_CODE",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT8,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_FIELD_VLAN_TAGS,
        .attridname             = "SAI_ACL_ENTRY_ATTR_FIELD_VLAN_TAGS",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_INT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = StringifyEnum ( sai_packet_vlan_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_packet_vlan_t ),
        .enummetadata           = &metadata_enum_sai_packet_vlan_t,
        .conditions             = { },
    },

    // TODO we need checks for all ranges


    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_FIELD_FDB_DST_USER_META,
        .attridname             = "SAI_ACL_ENTRY_ATTR_FIELD_FDB_DST_USER_META",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_FIELD_ROUTE_DST_USER_META,
        .attridname             = "SAI_ACL_ENTRY_ATTR_FIELD_ROUTE_DST_USER_META",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_FIELD_NEIGHBOR_USER_META,
        .attridname             = "SAI_ACL_ENTRY_ATTR_FIELD_NEIGHBOR_USER_META",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_FIELD_PORT_USER_META,
        .attridname             = "SAI_ACL_ENTRY_ATTR_FIELD_PORT_USER_META",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_FIELD_VLAN_USER_META,
        .attridname             = "SAI_ACL_ENTRY_ATTR_FIELD_VLAN_USER_META",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_FIELD_ACL_USER_META,
        .attridname             = "SAI_ACL_ENTRY_ATTR_FIELD_ACL_USER_META",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_FIELD_FDB_NPU_META_DST_HIT,
        .attridname             = "SAI_ACL_ENTRY_ATTR_FIELD_FDB_NPU_META_DST_HIT",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_BOOL,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_FIELD_NEIGHBOR_NPU_META_DST_HIT,
        .attridname             = "SAI_ACL_ENTRY_ATTR_FIELD_NEIGHBOR_NPU_META_DST_HIT",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_BOOL,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_FIELD_ROUTE_NPU_META_DST_HIT,
        .attridname             = "SAI_ACL_ENTRY_ATTR_FIELD_ROUTE_NPU_META_DST_HIT",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_BOOL,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    // TODO SAI_ACL_ENTRY_ATTR_USER_DEFINED_FIELD_MIN .. SAI_ACL_ENTRY_ATTR_USER_DEFINED_FIELD_MAX

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_FIELD_RANGE,
        .attridname             = "SAI_ACL_ENTRY_ATTR_FIELD_RANGE",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_OBJECT_LIST,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_ACL_RANGE },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_EMPTY_LIST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    // ACL ENTRY ACTIONS

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_ACTION_REDIRECT,
        .attridname             = "SAI_ACL_ENTRY_ATTR_ACTION_REDIRECT",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_PORT, SAI_OBJECT_TYPE_LAG, SAI_OBJECT_TYPE_NEXT_HOP, SAI_OBJECT_TYPE_NEXT_HOP_GROUP },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .oid = SAI_NULL_OBJECT_ID },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_ACTION_REDIRECT_LIST,
        .attridname             = "SAI_ACL_ENTRY_ATTR_ACTION_REDIRECT_LIST",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_LIST,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_PORT, SAI_OBJECT_TYPE_LAG, SAI_OBJECT_TYPE_NEXT_HOP, SAI_OBJECT_TYPE_NEXT_HOP_GROUP },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_EMPTY_LIST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_PACKET_ACTION,
        .attridname             = "SAI_ACL_ENTRY_ATTR_PACKET_ACTION",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_INT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = StringifyEnum ( sai_packet_action_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_packet_action_t ),
        .enummetadata           = &metadata_enum_sai_packet_action_t,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_ACTION_FLOOD,
        .attridname             = "SAI_ACL_ENTRY_ATTR_ACTION_FLOOD",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_INT32, // TODO what type? parameter not needed?
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_ACTION_COUNTER,
        .attridname             = "SAI_ACL_ENTRY_ATTR_ACTION_COUNTER",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_ACL_COUNTER },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_ACTION_MIRROR_INGRESS,
        .attridname             = "SAI_ACL_ENTRY_ATTR_ACTION_MIRROR_INGRESS",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_LIST,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_MIRROR },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_EMPTY_LIST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },

        // extra condition maybe needed to check on session
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_ACTION_MIRROR_EGRESS,
        .attridname             = "SAI_ACL_ENTRY_ATTR_ACTION_MIRROR_EGRESS",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_LIST,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_MIRROR },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_EMPTY_LIST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },

        // extra condition maybe needed to check on session
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_ACTION_SET_POLICER,
        .attridname             = "SAI_ACL_ENTRY_ATTR_ACTION_SET_POLICER",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_POLICER },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_ACTION_DECREMENT_TTL,
        .attridname             = "SAI_ACL_ENTRY_ATTR_ACTION_DECREMENT_TTL",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_INT32, // TODO parameter not needed?
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_ACTION_SET_TC,
        .attridname             = "SAI_ACL_ENTRY_ATTR_ACTION_SET_TC",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_UINT8,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_ACTION_SET_COLOR,
        .attridname             = "SAI_ACL_ENTRY_ATTR_ACTION_SET_COLOR",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_INT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = StringifyEnum ( sai_packet_color_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_packet_color_t ),
        .enummetadata           = &metadata_enum_sai_packet_color_t,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_ACTION_SET_INNER_VLAN_ID,
        .attridname             = "SAI_ACL_ENTRY_ATTR_ACTION_SET_INNER_VLAN_ID",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_UINT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },

        // TODO should this vlan exist? should it be object id?
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_ACTION_SET_INNER_VLAN_PRI,
        .attridname             = "SAI_ACL_ENTRY_ATTR_ACTION_SET_INNER_VLAN_PRI",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_UINT8,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_ACTION_SET_OUTER_VLAN_ID,
        .attridname             = "SAI_ACL_ENTRY_ATTR_ACTION_SET_OUTER_VLAN_ID",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_UINT16,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },

        // TODO should this vlan exist? should it be object id?
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_ACTION_SET_OUTER_VLAN_PRI,
        .attridname             = "SAI_ACL_ENTRY_ATTR_ACTION_SET_OUTER_VLAN_PRI",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_UINT8,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_ACTION_SET_SRC_MAC,
        .attridname             = "SAI_ACL_ENTRY_ATTR_ACTION_SET_SRC_MAC",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_MAC,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_ACTION_SET_DST_MAC,
        .attridname             = "SAI_ACL_ENTRY_ATTR_ACTION_SET_DST_MAC",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_MAC,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_ACTION_SET_SRC_IP,
        .attridname             = "SAI_ACL_ENTRY_ATTR_ACTION_SET_SRC_IP",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_IPV4,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_ACTION_SET_DST_IP,
        .attridname             = "SAI_ACL_ENTRY_ATTR_ACTION_SET_DST_IP",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_IPV4,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_ACTION_SET_SRC_IPv6,
        .attridname             = "SAI_ACL_ENTRY_ATTR_ACTION_SET_SRC_IPv6",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_IPV6,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_ACTION_SET_DST_IPv6,
        .attridname             = "SAI_ACL_ENTRY_ATTR_ACTION_SET_DST_IPv6",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_IPV6,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_ACTION_SET_DSCP,
        .attridname             = "SAI_ACL_ENTRY_ATTR_ACTION_SET_DSCP",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_UINT8,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_ACTION_SET_ECN,
        .attridname             = "SAI_ACL_ENTRY_ATTR_ACTION_SET_ECN",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_UINT8,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_ACTION_SET_L4_SRC_PORT,
        .attridname             = "SAI_ACL_ENTRY_ATTR_ACTION_SET_L4_SRC_PORT",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_UINT16,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_ACTION_SET_L4_DST_PORT,
        .attridname             = "SAI_ACL_ENTRY_ATTR_ACTION_SET_L4_DST_PORT",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_UINT16,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_ACTION_INGRESS_SAMPLEPACKET_ENABLE,
        .attridname             = "SAI_ACL_ENTRY_ATTR_ACTION_INGRESS_SAMPLEPACKET_ENABLE",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_SAMPLEPACKET },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_ACTION_EGRESS_SAMPLEPACKET_ENABLE,
        .attridname             = "SAI_ACL_ENTRY_ATTR_ACTION_EGRESS_SAMPLEPACKET_ENABLE",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_SAMPLEPACKET },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_ACTION_SET_CPU_QUEUE,
        .attridname             = "SAI_ACL_ENTRY_ATTR_ACTION_SET_CPU_QUEUE",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_QUEUE },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_ACTION_SET_ACL_META_DATA,
        .attridname             = "SAI_ACL_ENTRY_ATTR_ACTION_SET_ACL_META_DATA",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_UINT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_ACTION_EGRESS_BLOCK_PORT_LIST,
        .attridname             = "SAI_ACL_ENTRY_ATTR_ACTION_EGRESS_BLOCK_PORT_LIST",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_LIST,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_PORT },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_EMPTY_LIST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_ENTRY,
        .attrid                 = SAI_ACL_ENTRY_ATTR_ACTION_SET_USER_TRAP_ID,
        .attridname             = "SAI_ACL_ENTRY_ATTR_ACTION_SET_USER_TRAP_ID",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_UINT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },

        // range in SAI_SWITCH_ATTR_ACL_USER_TRAP_ID_RANGE
    },
};

const size_t sai_acl_entry_attr_metadata_count = sizeof(sai_acl_entry_attr_metadata)/sizeof(sai_attr_metadata_t);

// METADATA for SAI_OBJECT_TYPE_ACL_COUNTER

const sai_attr_metadata_t sai_acl_counter_attr_metadata[] = {

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_COUNTER,
        .attrid                 = SAI_ACL_COUNTER_ATTR_TABLE_ID,
        .attridname             = "SAI_ACL_COUNTER_ATTR_TABLE_ID",
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_ACL_TABLE },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_COUNTER,
        .attrid                 = SAI_ACL_COUNTER_ATTR_ENABLE_PACKET_COUNT,
        .attridname             = "SAI_ACL_COUNTER_ATTR_ENABLE_PACKET_COUNT",
        .serializationtype      = SAI_SERIALIZATION_TYPE_BOOL,
        .flags                  = SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .booldata = false },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_COUNTER,
        .attrid                 = SAI_ACL_COUNTER_ATTR_ENABLE_BYTE_COUNT,
        .attridname             = "SAI_ACL_COUNTER_ATTR_ENABLE_BYTE_COUNT",
        .serializationtype      = SAI_SERIALIZATION_TYPE_BOOL,
        .flags                  = SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .booldata = false },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_COUNTER,
        .attrid                 = SAI_ACL_COUNTER_ATTR_PACKETS,
        .attridname             = "SAI_ACL_COUNTER_ATTR_PACKETS",
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT64,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .u64 = 0 },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },

        // dynamic
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_COUNTER,
        .attrid                 = SAI_ACL_COUNTER_ATTR_BYTES,
        .attridname             = "SAI_ACL_COUNTER_ATTR_BYTES",
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT64,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .u64 = 0 },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },

        // dynamic
    },
};

const size_t sai_acl_counter_attr_metadata_count = sizeof(sai_acl_counter_attr_metadata)/sizeof(sai_attr_metadata_t);

// METADATA for SAI_OBJECT_TYPE_ACL_RANGE

const sai_attr_metadata_t sai_acl_range_attr_metadata[] = {

    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_RANGE,
        .attrid                 = SAI_ACL_RANGE_ATTR_TYPE,
        .attridname             = "SAI_ACL_RANGE_ATTR_TYPE",
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = StringifyEnum ( sai_acl_range_type_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_acl_range_type_t ),
        .enummetadata           = &metadata_enum_sai_acl_range_type_t,
        .conditions             = { },
    },
    {
        .objecttype             = SAI_OBJECT_TYPE_ACL_RANGE,
        .attrid                 = SAI_ACL_RANGE_ATTR_LIMIT,
        .attridname             = "SAI_ACL_RANGE_ATTR_LIMIT",
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT32_RANGE,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    }
};

const size_t sai_acl_range_attr_metadata_count = sizeof(sai_acl_range_attr_metadata)/sizeof(sai_attr_metadata_t);
