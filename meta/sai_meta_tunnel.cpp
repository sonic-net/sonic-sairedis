#include "sai_meta.h"

// METADATA for SAI_OBJECT_TYPE_TUNNEL

DEFINE_ENUM_VALUES(sai_tunnel_type_t)
{
    SAI_TUNNEL_IPINIP,
    SAI_TUNNEL_IPINIP_GRE,
    SAI_TUNNEL_VXLAN,
    SAI_TUNNEL_MPLS
};

DEFINE_ENUM_VALUES(sai_tunnel_ttl_mode_t)
{
    SAI_TUNNEL_TTL_UNIFORM_MODEL,
    SAI_TUNNEL_TTL_PIPE_MODEL
};

DEFINE_ENUM_VALUES(sai_tunnel_dscp_mode_t)
{
    SAI_TUNNEL_DSCP_UNIFORM_MODEL,
    SAI_TUNNEL_DSCP_PIPE_MODEL
};

DEFINE_ENUM_VALUES(sai_tunnel_encap_ecn_mode_t)
{
    SAI_TUNNEL_ENCAP_ECN_MODE_STANDARD,
    SAI_TUNNEL_ENCAP_ECN_MODE_USER_DEFINED
};

DEFINE_ENUM_VALUES(sai_tunnel_decap_ecn_mode_t)
{
    SAI_TUNNEL_DECAP_ECN_MODE_STANDARD,
    SAI_TUNNEL_DECAP_ECN_MODE_COPY_FROM_OUTER,
    SAI_TUNNEL_DECAP_ECN_MODE_USER_DEFINED
};

DEFINE_ENUM_VALUES(sai_tunnel_map_type_t)
{
    SAI_TUNNEL_MAP_OECN_TO_UECN,
    SAI_TUNNEL_MAP_UECN_OECN_TO_OECN,
    SAI_TUNNEL_MAP_VNI_TO_VLAN_ID,
    SAI_TUNNEL_MAP_VLAN_ID_TO_VNI
};

DEFINE_ENUM_VALUES(sai_tunnel_term_table_entry_type_t)
{
    SAI_TUNNEL_TERM_TABLE_ENTRY_P2P,
    SAI_TUNNEL_TERM_TABLE_ENTRY_P2MP
};

const sai_attr_metadata_t sai_tunnel_attr_metadata[] = {

    {
        .objecttype             = SAI_OBJECT_TYPE_TUNNEL,
        .attrid                 = SAI_TUNNEL_ATTR_TYPE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = StringifyEnum ( sai_tunnel_type_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_tunnel_type_t ),
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_TUNNEL,
        .attrid                 = SAI_TUNNEL_ATTR_UNDERLAY_INTERFACE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_ROUTER_INTERFACE },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = {
                                        COND_ENUM ( SAI_TUNNEL_ATTR_TYPE, SAI_TUNNEL_IPINIP ),
                                        COND_ENUM ( SAI_TUNNEL_ATTR_TYPE, SAI_TUNNEL_IPINIP_GRE ),
                                  },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_TUNNEL,
        .attrid                 = SAI_TUNNEL_ATTR_OVERLAY_INTERFACE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_ROUTER_INTERFACE },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = {
                                        COND_ENUM ( SAI_TUNNEL_ATTR_TYPE, SAI_TUNNEL_IPINIP ),
                                        COND_ENUM ( SAI_TUNNEL_ATTR_TYPE, SAI_TUNNEL_IPINIP_GRE ),
                                  },
    },

    // TUNNEL ENCAP ATTRIBUTES

    {
        .objecttype             = SAI_OBJECT_TYPE_TUNNEL,
        .attrid                 = SAI_TUNNEL_ATTR_ENCAP_SRC_IP,
        .serializationtype      = SAI_SERIALIZATION_TYPE_IP_ADDRESS,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_TUNNEL,
        .attrid                 = SAI_TUNNEL_ATTR_ENCAP_TTL_MODE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .s32 = SAI_TUNNEL_TTL_UNIFORM_MODEL },
        .enumtypestr            = StringifyEnum ( sai_tunnel_ttl_mode_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_tunnel_ttl_mode_t ),
        .conditions             = { },

        // TODO some extra checks may be needed
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_TUNNEL,
        .attrid                 = SAI_TUNNEL_ATTR_ENCAP_TTL_VAL,
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT8,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { COND_ENUM ( SAI_TUNNEL_ATTR_ENCAP_TTL_MODE, SAI_TUNNEL_TTL_PIPE_MODEL ) },

        // TODO valid only and MANDATORY_ON_CREATE when SAI_TUNNEL_ENCAP_TTL_MODE = SAI_TUNNEL_TTL_PIPE_MODEL
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_TUNNEL,
        .attrid                 = SAI_TUNNEL_ATTR_ENCAP_DSCP_MODE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .s32 = SAI_TUNNEL_DSCP_UNIFORM_MODEL },
        .enumtypestr            = StringifyEnum ( sai_tunnel_dscp_mode_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_tunnel_dscp_mode_t ),
        .conditions             = { }
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_TUNNEL,
        .attrid                 = SAI_TUNNEL_ATTR_ENCAP_DSCP_VAL,
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT8,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { COND_ENUM ( SAI_TUNNEL_ATTR_ENCAP_DSCP_MODE, SAI_TUNNEL_DSCP_PIPE_MODEL ) },

        // TODO only 6 bits, additional check is needed
        // TODO valid only and MANDATORY_ON_CREATE when SAI_TUNNEL_ENCAP_DSCP_MODE = SAI_TUNNEL_DSCP_PIPE_MODEL
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_TUNNEL,
        .attrid                 = SAI_TUNNEL_ATTR_ENCAP_GRE_KEY_VALID,
        .serializationtype      = SAI_SERIALIZATION_TYPE_BOOL,
        .flags                  = SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .booldata = false },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { COND_ENUM ( SAI_TUNNEL_ATTR_TYPE, SAI_TUNNEL_IPINIP_GRE ) },

        // valid only on that condition
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_TUNNEL,
        .attrid                 = SAI_TUNNEL_ATTR_ENCAP_GRE_KEY,
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT32,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { COND_BOOL ( SAI_TUNNEL_ATTR_ENCAP_GRE_KEY_VALID, true) },

        // valid only on that condition
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_TUNNEL,
        .attrid                 = SAI_TUNNEL_ATTR_ENCAP_ECN_MODE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .s32 = SAI_TUNNEL_ENCAP_ECN_MODE_STANDARD },
        .enumtypestr            = StringifyEnum ( sai_tunnel_encap_ecn_mode_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_tunnel_encap_ecn_mode_t ),
        .conditions             = { },

        // TODO Need to provide SAI_TUNNEL_MAP_OECN_TO_UECN in SAI_TUNNEL_ATTR_ENCAP_MAPPERS
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_TUNNEL,
        .attrid                 = SAI_TUNNEL_ATTR_ENCAP_MAPPERS,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_LIST,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_TUNNEL_MAP },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { COND_ENUM ( SAI_TUNNEL_ATTR_ENCAP_ECN_MODE, SAI_TUNNEL_ENCAP_ECN_MODE_USER_DEFINED ) },

        // TODO extra logic will be needed on mappers
        // TODO valid only on that condition
    },

    // TUNNEL DECAP ATTRIBUTES

    {
        .objecttype             = SAI_OBJECT_TYPE_TUNNEL,
        .attrid                 = SAI_TUNNEL_ATTR_DECAP_ECN_MODE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .s32 = SAI_TUNNEL_DECAP_ECN_MODE_STANDARD },
        .enumtypestr            = StringifyEnum ( sai_tunnel_decap_ecn_mode_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_tunnel_decap_ecn_mode_t ),
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_TUNNEL,
        .attrid                 = SAI_TUNNEL_ATTR_DECAP_MAPPERS,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_LIST,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_TUNNEL_MAP },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { COND_ENUM ( SAI_TUNNEL_ATTR_DECAP_ECN_MODE, SAI_TUNNEL_DECAP_ECN_MODE_USER_DEFINED ) },

        // TODO extra logic will be needed on mappers
        // valid only on that condition
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_TUNNEL,
        .attrid                 = SAI_TUNNEL_ATTR_DECAP_TTL_MODE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { }, //{ .s32 = SAI_TUNNEL_TTL_UNIFORM_MODEL },
        .enumtypestr            = StringifyEnum ( sai_tunnel_ttl_mode_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_tunnel_ttl_mode_t ),
        .conditions             = {
                                        COND_ENUM ( SAI_TUNNEL_ATTR_TYPE, SAI_TUNNEL_IPINIP ),
                                        COND_ENUM ( SAI_TUNNEL_ATTR_TYPE, SAI_TUNNEL_IPINIP_GRE ),
                                  },

        // TODO condition is more complex since once is valid and once is mandatory
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_TUNNEL,
        .attrid                 = SAI_TUNNEL_ATTR_DECAP_DSCP_MODE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { }, // { .s32 = SAI_TUNNEL_DSCP_UNIFORM_MODEL },
        .enumtypestr            = StringifyEnum ( sai_tunnel_dscp_mode_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_tunnel_dscp_mode_t ),
        .conditions             = {
                                        COND_ENUM ( SAI_TUNNEL_ATTR_TYPE, SAI_TUNNEL_IPINIP ),
                                        COND_ENUM ( SAI_TUNNEL_ATTR_TYPE, SAI_TUNNEL_IPINIP_GRE ),
                                  },

        // TODO condition is more complex since once is valid and once is mandatory
    },

};

const size_t sai_tunnel_attr_metadata_count = sizeof(sai_tunnel_attr_metadata)/sizeof(sai_attr_metadata_t);

// METADATA for SAI_OBJECT_TYPE_TUNNEL_MAP

const sai_attr_metadata_t sai_tunnel_map_attr_metadata[] = {

    {
        .objecttype             = SAI_OBJECT_TYPE_TUNNEL_MAP,
        .attrid                 = SAI_TUNNEL_MAP_ATTR_TYPE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = StringifyEnum ( sai_tunnel_map_type_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_tunnel_map_type_t ),
        .conditions             = { },

        // TODO some extra logic may be needed
    },

//    {
//        .objecttype             = SAI_OBJECT_TYPE_TUNNEL_MAP,
//        .attrid                 = SAI_TUNNEL_MAP_ATTR_MAP_TO_VALUE_LIST,
//        .serializationtype      = SAI_SERIALIZATION_TYPE_TUNNEL_MAP_LIST,   // TODO declare
//        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
//        .allowedobjecttypes     = { },
//        .allownullobjectid      = false,
//        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
//        .defaultvalue           = { },
//        .enumtypestr            = NULL,
//        .enumallowedvalues      = { },
//        .conditions             = { },
//
//        // TODO some extra logic may be needed on map
//    },

};

const size_t sai_tunnel_map_attr_metadata_count = sizeof(sai_tunnel_map_attr_metadata)/sizeof(sai_attr_metadata_t);

// METADATA for SAI_OBJECT_TYPE_TUNNEL_TABLE_ENTRY

const sai_attr_metadata_t sai_tunnel_table_entry_attr_metadata[] = {

    {
        .objecttype             = SAI_OBJECT_TYPE_TUNNEL_TABLE_ENTRY,
        .attrid                 = SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_VR_ID,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_VIRTUAL_ROUTER },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_TUNNEL_TABLE_ENTRY,
        .attrid                 = SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_TYPE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = StringifyEnum ( sai_tunnel_term_table_entry_type_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_tunnel_term_table_entry_type_t ),
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_TUNNEL_TABLE_ENTRY,
        .attrid                 = SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_DST_IP,
        .serializationtype      = SAI_SERIALIZATION_TYPE_IP_ADDRESS,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_TUNNEL_TABLE_ENTRY,
        .attrid                 = SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_SRC_IP,
        .serializationtype      = SAI_SERIALIZATION_TYPE_IP_ADDRESS,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { COND_ENUM ( SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_TYPE, SAI_TUNNEL_TERM_TABLE_ENTRY_P2P ) },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_TUNNEL_TABLE_ENTRY,
        .attrid                 = SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_TUNNEL_TYPE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = StringifyEnum ( sai_tunnel_type_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_tunnel_type_t ),
        .conditions             = { COND_ENUM ( SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_TYPE, SAI_TUNNEL_TERM_TABLE_ENTRY_P2P ) },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_TUNNEL_TABLE_ENTRY,
        .attrid                 = SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_ACTION_TUNNEL_ID,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_TUNNEL },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },

        // TODO some additional logic may be needed for tunnel type IPinIP etc
    },
};

const size_t sai_tunnel_table_entry_attr_metadata_count = sizeof(sai_tunnel_table_entry_attr_metadata)/sizeof(sai_attr_metadata_t);
