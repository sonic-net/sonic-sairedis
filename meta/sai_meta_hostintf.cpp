#include "sai_meta.h"

// NOTE: user defined traps are subject to be removed

// METADATA for SAI_OBJECT_TYPE_HOST_INTERFACE

DEFINE_ENUM_VALUES(sai_hostif_type_t)
{
    SAI_HOSTIF_TYPE_NETDEV,
    SAI_HOSTIF_TYPE_FD
};

DEFINE_ENUM_VALUES(sai_hostif_trap_id_t)
{
    SAI_HOSTIF_TRAP_ID_STP,
    SAI_HOSTIF_TRAP_ID_LACP,
    SAI_HOSTIF_TRAP_ID_EAPOL,
    SAI_HOSTIF_TRAP_ID_LLDP,
    SAI_HOSTIF_TRAP_ID_PVRST,
    SAI_HOSTIF_TRAP_ID_IGMP_TYPE_QUERY,
    SAI_HOSTIF_TRAP_ID_IGMP_TYPE_LEAVE,
    SAI_HOSTIF_TRAP_ID_IGMP_TYPE_V1_REPORT,
    SAI_HOSTIF_TRAP_ID_IGMP_TYPE_V2_REPORT,
    SAI_HOSTIF_TRAP_ID_IGMP_TYPE_V3_REPORT,
    SAI_HOSTIF_TRAP_ID_SAMPLEPACKET,
    SAI_HOSTIF_TRAP_ID_ARP_REQUEST,
    SAI_HOSTIF_TRAP_ID_ARP_RESPONSE,
    SAI_HOSTIF_TRAP_ID_DHCP,
    SAI_HOSTIF_TRAP_ID_OSPF,
    SAI_HOSTIF_TRAP_ID_PIM,
    SAI_HOSTIF_TRAP_ID_VRRP,
    SAI_HOSTIF_TRAP_ID_BGP,
    SAI_HOSTIF_TRAP_ID_DHCPV6,
    SAI_HOSTIF_TRAP_ID_OSPFV6,
    SAI_HOSTIF_TRAP_ID_VRRPV6,
    SAI_HOSTIF_TRAP_ID_BGPV6,
    SAI_HOSTIF_TRAP_ID_IPV6_NEIGHBOR_DISCOVERY,
    SAI_HOSTIF_TRAP_ID_IPV6_MLD_V1_V2,
    SAI_HOSTIF_TRAP_ID_IPV6_MLD_V1_REPORT,
    SAI_HOSTIF_TRAP_ID_IPV6_MLD_V1_DONE,
    SAI_HOSTIF_TRAP_ID_MLD_V2_REPORT,
    SAI_HOSTIF_TRAP_ID_IP2ME,
    SAI_HOSTIF_TRAP_ID_SSH,
    SAI_HOSTIF_TRAP_ID_SNMP,
    SAI_HOSTIF_TRAP_ID_L3_MTU_ERROR,
    SAI_HOSTIF_TRAP_ID_TTL_ERROR
};

DEFINE_ENUM_VALUES(sai_hostif_trap_channel_t)
{
    SAI_HOSTIF_TRAP_CHANNEL_FD,
    SAI_HOSTIF_TRAP_CHANNEL_CB,
    SAI_HOSTIF_TRAP_CHANNEL_NETDEV
};

const char metadata_sai_hostif_trap_channel_t_enum_name[] = "sai_hostif_trap_channel_t";
const sai_hostif_trap_channel_t metadata_sai_hostif_trap_channel_t_enum_values[] = {
    SAI_HOSTIF_TRAP_CHANNEL_FD,
    SAI_HOSTIF_TRAP_CHANNEL_CB,
    SAI_HOSTIF_TRAP_CHANNEL_NETDEV,
};
const char* metadata_sai_hostif_trap_channel_t_enum_values_names[] = {
    "SAI_HOSTIF_TRAP_CHANNEL_FD",
    "SAI_HOSTIF_TRAP_CHANNEL_CB",
    "SAI_HOSTIF_TRAP_CHANNEL_NETDEV",
    NULL
};
const char* metadata_sai_hostif_trap_channel_t_enum_values_short_names[] = {
    "FD",
    "CB",
    "NETDEV",
    NULL
};
const size_t metadata_sai_hostif_trap_channel_t_enum_values_count = 3;
DEFINE_ENUM_METADATA(sai_hostif_trap_channel_t, 3);

const char metadata_sai_hostif_type_t_enum_name[] = "sai_hostif_type_t";
const sai_hostif_type_t metadata_sai_hostif_type_t_enum_values[] = {
    SAI_HOSTIF_TYPE_NETDEV,
    SAI_HOSTIF_TYPE_FD,
};
const char* metadata_sai_hostif_type_t_enum_values_names[] = {
    "SAI_HOSTIF_TYPE_NETDEV",
    "SAI_HOSTIF_TYPE_FD",
    NULL
};
const char* metadata_sai_hostif_type_t_enum_values_short_names[] = {
    "NETDEV",
    "FD",
    NULL
};
const size_t metadata_sai_hostif_type_t_enum_values_count = 2;
DEFINE_ENUM_METADATA(sai_hostif_type_t, 2);

const char metadata_sai_hostif_trap_type_t_enum_name[] = "sai_hostif_trap_type_t";
const sai_hostif_trap_id_t metadata_sai_hostif_trap_type_t_enum_values[] = {
    SAI_HOSTIF_TRAP_ID_STP,
    SAI_HOSTIF_TRAP_ID_LACP,
    SAI_HOSTIF_TRAP_ID_EAPOL,
    SAI_HOSTIF_TRAP_ID_LLDP,
    SAI_HOSTIF_TRAP_ID_PVRST,
    SAI_HOSTIF_TRAP_ID_IGMP_TYPE_QUERY,
    SAI_HOSTIF_TRAP_ID_IGMP_TYPE_LEAVE,
    SAI_HOSTIF_TRAP_ID_IGMP_TYPE_V1_REPORT,
    SAI_HOSTIF_TRAP_ID_IGMP_TYPE_V2_REPORT,
    SAI_HOSTIF_TRAP_ID_IGMP_TYPE_V3_REPORT,
    SAI_HOSTIF_TRAP_ID_SAMPLEPACKET,
    SAI_HOSTIF_TRAP_ID_ARP_REQUEST,
    SAI_HOSTIF_TRAP_ID_ARP_RESPONSE,
    SAI_HOSTIF_TRAP_ID_DHCP,
    SAI_HOSTIF_TRAP_ID_OSPF,
    SAI_HOSTIF_TRAP_ID_PIM,
    SAI_HOSTIF_TRAP_ID_VRRP,
    SAI_HOSTIF_TRAP_ID_BGP,
    SAI_HOSTIF_TRAP_ID_DHCPV6,
    SAI_HOSTIF_TRAP_ID_OSPFV6,
    SAI_HOSTIF_TRAP_ID_VRRPV6,
    SAI_HOSTIF_TRAP_ID_BGPV6,
    SAI_HOSTIF_TRAP_ID_IPV6_NEIGHBOR_DISCOVERY,
    SAI_HOSTIF_TRAP_ID_IPV6_MLD_V1_V2,
    SAI_HOSTIF_TRAP_ID_IPV6_MLD_V1_REPORT,
    SAI_HOSTIF_TRAP_ID_IPV6_MLD_V1_DONE,
    SAI_HOSTIF_TRAP_ID_MLD_V2_REPORT,
    SAI_HOSTIF_TRAP_ID_IP2ME,
    SAI_HOSTIF_TRAP_ID_SSH,
    SAI_HOSTIF_TRAP_ID_SNMP,
    SAI_HOSTIF_TRAP_ID_L3_MTU_ERROR,
    SAI_HOSTIF_TRAP_ID_TTL_ERROR,
    SAI_HOSTIF_TRAP_ID_CUSTOM_EXCEPTION_RANGE_BASE,
};
const char* metadata_sai_hostif_trap_type_t_enum_values_names[] = {
    "SAI_HOSTIF_TRAP_TYPE_STP",
    "SAI_HOSTIF_TRAP_TYPE_LACP",
    "SAI_HOSTIF_TRAP_TYPE_EAPOL",
    "SAI_HOSTIF_TRAP_TYPE_LLDP",
    "SAI_HOSTIF_TRAP_TYPE_PVRST",
    "SAI_HOSTIF_TRAP_TYPE_IGMP_TYPE_QUERY",
    "SAI_HOSTIF_TRAP_TYPE_IGMP_TYPE_LEAVE",
    "SAI_HOSTIF_TRAP_TYPE_IGMP_TYPE_V1_REPORT",
    "SAI_HOSTIF_TRAP_TYPE_IGMP_TYPE_V2_REPORT",
    "SAI_HOSTIF_TRAP_TYPE_IGMP_TYPE_V3_REPORT",
    "SAI_HOSTIF_TRAP_TYPE_SAMPLEPACKET",
    "SAI_HOSTIF_TRAP_TYPE_ARP_REQUEST",
    "SAI_HOSTIF_TRAP_TYPE_ARP_RESPONSE",
    "SAI_HOSTIF_TRAP_TYPE_DHCP",
    "SAI_HOSTIF_TRAP_TYPE_OSPF",
    "SAI_HOSTIF_TRAP_TYPE_PIM",
    "SAI_HOSTIF_TRAP_TYPE_VRRP",
    "SAI_HOSTIF_TRAP_TYPE_BGP",
    "SAI_HOSTIF_TRAP_TYPE_DHCPV6",
    "SAI_HOSTIF_TRAP_TYPE_OSPFV6",
    "SAI_HOSTIF_TRAP_TYPE_VRRPV6",
    "SAI_HOSTIF_TRAP_TYPE_BGPV6",
    "SAI_HOSTIF_TRAP_TYPE_IPV6_NEIGHBOR_DISCOVERY",
    "SAI_HOSTIF_TRAP_TYPE_IPV6_MLD_V1_V2",
    "SAI_HOSTIF_TRAP_TYPE_IPV6_MLD_V1_REPORT",
    "SAI_HOSTIF_TRAP_TYPE_IPV6_MLD_V1_DONE",
    "SAI_HOSTIF_TRAP_TYPE_MLD_V2_REPORT",
    "SAI_HOSTIF_TRAP_TYPE_IP2ME",
    "SAI_HOSTIF_TRAP_TYPE_SSH",
    "SAI_HOSTIF_TRAP_TYPE_SNMP",
    "SAI_HOSTIF_TRAP_TYPE_L3_MTU_ERROR",
    "SAI_HOSTIF_TRAP_TYPE_TTL_ERROR",
    "SAI_HOSTIF_TRAP_TYPE_CUSTOM_EXCEPTION_RANGE_BASE",
    NULL
};
const char* metadata_sai_hostif_trap_type_t_enum_values_short_names[] = {
    "STP",
    "LACP",
    "EAPOL",
    "LLDP",
    "PVRST",
    "IGMP_TYPE_QUERY",
    "IGMP_TYPE_LEAVE",
    "IGMP_TYPE_V1_REPORT",
    "IGMP_TYPE_V2_REPORT",
    "IGMP_TYPE_V3_REPORT",
    "SAMPLEPACKET",
    "ARP_REQUEST",
    "ARP_RESPONSE",
    "DHCP",
    "OSPF",
    "PIM",
    "VRRP",
    "BGP",
    "DHCPV6",
    "OSPFV6",
    "VRRPV6",
    "BGPV6",
    "IPV6_NEIGHBOR_DISCOVERY",
    "IPV6_MLD_V1_V2",
    "IPV6_MLD_V1_REPORT",
    "IPV6_MLD_V1_DONE",
    "MLD_V2_REPORT",
    "IP2ME",
    "SSH",
    "SNMP",
    "L3_MTU_ERROR",
    "TTL_ERROR",
    "CUSTOM_EXCEPTION_RANGE_BASE",
    NULL
};
const size_t metadata_sai_hostif_trap_type_t_enum_values_count = 33;
DEFINE_ENUM_METADATA(sai_hostif_trap_type_t, 33);

const sai_attr_metadata_t sai_hostintf_attr_metadata[] = {

    {
        .objecttype             = SAI_OBJECT_TYPE_HOST_INTERFACE,
        .attrid                 = SAI_HOSTIF_ATTR_TYPE,
        .attridname             = "SAI_HOSTIF_ATTR_TYPE",
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = StringifyEnum ( sai_hostif_type_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_hostif_type_t ),
        .enummetadata           = &metadata_enum_sai_hostif_type_t,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_HOST_INTERFACE,
        .attrid                 = SAI_HOSTIF_ATTR_RIF_OR_PORT_ID,
        .attridname             = "SAI_HOSTIF_ATTR_RIF_OR_PORT_ID",
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_PORT, SAI_OBJECT_TYPE_ROUTER_INTERFACE },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { COND_ENUM ( SAI_HOSTIF_ATTR_TYPE, SAI_HOSTIF_TYPE_NETDEV ) },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_HOST_INTERFACE,
        .attrid                 = SAI_HOSTIF_ATTR_NAME,
        .attridname             = "SAI_HOSTIF_ATTR_NAME",
        .serializationtype      = SAI_SERIALIZATION_TYPE_CHARDATA,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { COND_ENUM ( SAI_HOSTIF_ATTR_TYPE, SAI_HOSTIF_TYPE_NETDEV ) },

        // TODO extra logic on name validation is needed
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_HOST_INTERFACE,
        .attrid                 = SAI_HOSTIF_ATTR_OPER_STATUS,
        .attridname             = "SAI_HOSTIF_ATTR_OPER_STATUS",
        .serializationtype      = SAI_SERIALIZATION_TYPE_BOOL,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .booldata = false },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },
};

const size_t sai_hostintf_attr_metadata_count = sizeof(sai_hostintf_attr_metadata)/sizeof(sai_attr_metadata_t);

// METADATA for SAI_OBJECT_TYPE_TRAP_GROUP

const sai_attr_metadata_t sai_hostintf_trap_group_attr_metadata[] = {

    {
        .objecttype             = SAI_OBJECT_TYPE_TRAP_GROUP,
        .attrid                 = SAI_HOSTIF_TRAP_GROUP_ATTR_ADMIN_STATE,
        .attridname             = "SAI_HOSTIF_TRAP_GROUP_ATTR_ADMIN_STATE",
        .serializationtype      = SAI_SERIALIZATION_TYPE_BOOL,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .booldata = true },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_TRAP_GROUP,
        .attrid                 = SAI_HOSTIF_TRAP_GROUP_ATTR_QUEUE,
        .attridname             = "SAI_HOSTIF_TRAP_GROUP_ATTR_QUEUE",
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .u32 = 0 },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },

        // TODO this can be tricky since this QUEUE is queue INDEX
        // indirect depenency, by default there are 8 queues, but
        // user can create extra one, so there may be 10, and what
        // happens when this points to queue 10 and user remove this queue?
        // on queue remove we should queue index on trap group and
        // not allow to remove then
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_TRAP_GROUP,
        .attrid                 = SAI_HOSTIF_TRAP_GROUP_ATTR_POLICER,
        .attridname             = "SAI_HOSTIF_TRAP_GROUP_ATTR_POLICER",
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_POLICER },
        .allownullobjectid      = true,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .oid = SAI_NULL_OBJECT_ID },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },
};

const size_t sai_hostintf_trap_group_attr_metadata_count = sizeof(sai_hostintf_trap_group_attr_metadata)/sizeof(sai_attr_metadata_t);

// METADATA for SAI_OBJECT_TYPE_TRAP

const sai_attr_metadata_t sai_hostintf_trap_attr_metadata[] = {
/*
    {
        .objecttype             = SAI_OBJECT_TYPE_TRAP,
        .attrid                 = SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE,
        .nameid                 = "SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE",
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_AND_SET | SAI_ATTR_FLAGS_KEY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = StringifyEnum ( sai_hostif_trap_type_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_hostif_trap_type_f ),
        .enummetadata           = &metadata_enum_sai_hostif_trap_type_f,
        .conditions             = { },
    },
*/
    {
        .objecttype             = SAI_OBJECT_TYPE_TRAP,
        .attrid                 = SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION,
        .attridname             = "SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION",
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .s32 = SAI_PACKET_ACTION_FORWARD }, // TODO check if this is right one
        .enumtypestr            = StringifyEnum ( sai_packet_action_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_packet_action_t ),
        .enummetadata           = &metadata_enum_sai_packet_action_t,
        .conditions             = { },
    },

    // TODO below attributes are valid only when:
    // (those are not mandatory, but only valid, default action is assumed)
    // SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION == SAI_PACKET_ACTION_TRAP or
    // SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION == SAI_PACKET_ACTION_LOG

    // { COND_ENUM ( SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION, SAI_PACKET_ACTION_TRAP ) },
    // { COND_ENUM ( SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION, SAI_PACKET_ACTION_LOG ) },

    // if attribute is not set, but is "valid" then default value is used

    {
        .objecttype             = SAI_OBJECT_TYPE_TRAP,
        .attrid                 = SAI_HOSTIF_TRAP_ATTR_TRAP_PRIORITY,
        .attridname             = "SAI_HOSTIF_TRAP_ATTR_TRAP_PRIORITY",
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_ATTR_VALUE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },

        // TODO default value is provided by SAI_SWITCH_ATTR_ACL_ENTRY_MINIMUM_PRIORITY
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_TRAP,
        .attrid                 = SAI_HOSTIF_TRAP_ATTR_TRAP_CHANNEL,
        .attridname             = "SAI_HOSTIF_TRAP_ATTR_TRAP_CHANNEL",
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET, // mandatory + crete_only
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .s32 = SAI_HOSTIF_TRAP_CHANNEL_CB },
        .enumtypestr            = StringifyEnum ( sai_hostif_trap_channel_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_hostif_trap_channel_t ),
        .enummetadata           = &metadata_enum_sai_hostif_trap_channel_t,
        .conditions             = { },
    },
/*
    {
        .objecttype             = SAI_OBJECT_TYPE_TRAP,
        .attrid                 = SAI_HOSTIF_TRAP_ATTR_FD,
        .nameid                 = "SAI_HOSTIF_TRAP_ATTR_FD",
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },                          // TODO what kind of object ?
        .allownullobjectid      = true,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .oid = SAI_NULL_OBJECT_ID },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { }

         // TODO condition is more complex here since trap/log and this (extra logic needed)
         // * Valid only when SAI_HOSTIF_TRAP_ATTR_TRAP_CHANNEL == SAI_HOSTIF_TRAP_CHANNEL_FD
         // * Must be set before set SAI_HOSTIF_TRAP_ATTR_TRAP_CHANNEL to SAI_HOSTIF_TRAP_CHANNEL_FD
    },
*/
    {
        .objecttype             = SAI_OBJECT_TYPE_TRAP,
        .attrid                 = SAI_HOSTIF_TRAP_ATTR_PORT_LIST,
        .attridname             = "SAI_HOSTIF_TRAP_ATTR_PORT_LIST",
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_LIST,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_PORT },
        .allownullobjectid      = true,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_EMPTY_LIST,
        .defaultvalue           = { },                 // TODO default is to all ports
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { }

        // TODO extra logic is needed here since default is to all ports, we need more explanation
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_TRAP,
        .attrid                 = SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP,
        .attridname             = "SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP",
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET, // create only ?
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_TRAP_GROUP },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_ATTR_VALUE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { }

        // TODO default value points to SAI_SWITCH_ATTR_DEFAULT_TRAP_GROUP

        // TODO should null point to default trap group or should actual explicit object should be used
        // TODO extra logic is needed here
    },
};

const size_t sai_hostintf_trap_attr_metadata_count = sizeof(sai_hostintf_trap_attr_metadata)/sizeof(sai_attr_metadata_t);
