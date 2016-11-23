#include "sai_meta.h"

// METADATA for SAI_OBJECT_TYPE_MIRROR

DEFINE_ENUM_VALUES(sai_mirror_type_t)
{
    SAI_MIRROR_TYPE_LOCAL,
    SAI_MIRROR_TYPE_REMOTE,
    SAI_MIRROR_TYPE_ENHANCED_REMOTE
};

DEFINE_ENUM_VALUES(sai_erspan_encapsulation_type_t)
{
    SAI_MIRROR_L3_GRE_TUNNEL
};

const char metadata_sai_erspan_encapsulation_type_t_enum_name[] = "sai_erspan_encapsulation_type_t";
const sai_erspan_encapsulation_type_t metadata_sai_erspan_encapsulation_type_t_enum_values[] = {
    SAI_MIRROR_L3_GRE_TUNNEL,
};
const char* metadata_sai_erspan_encapsulation_type_t_enum_values_names[] = {
    "SAI_ERSPAN_ENCAPSULATION_TYPE_MIRROR_L3_GRE_TUNNEL",
    NULL
};
const char* metadata_sai_erspan_encapsulation_type_t_enum_values_short_names[] = {
    "MIRROR_L3_GRE_TUNNEL",
    NULL
};
const size_t metadata_sai_erspan_encapsulation_type_t_enum_values_count = 1;
DEFINE_ENUM_METADATA(sai_erspan_encapsulation_type_t, 1);

const char metadata_sai_mirror_type_t_enum_name[] = "sai_mirror_type_t";
const sai_mirror_type_t metadata_sai_mirror_type_t_enum_values[] = {
    SAI_MIRROR_TYPE_LOCAL,
    SAI_MIRROR_TYPE_REMOTE,
    SAI_MIRROR_TYPE_ENHANCED_REMOTE,
};
const char* metadata_sai_mirror_type_t_enum_values_names[] = {
    "SAI_MIRROR_TYPE_LOCAL",
    "SAI_MIRROR_TYPE_REMOTE",
    "SAI_MIRROR_TYPE_ENHANCED_REMOTE",
    NULL
};
const char* metadata_sai_mirror_type_t_enum_values_short_names[] = {
    "LOCAL",
    "REMOTE",
    "ENHANCED_REMOTE",
    NULL
};
const size_t metadata_sai_mirror_type_t_enum_values_count = 3;
DEFINE_ENUM_METADATA(sai_mirror_type_t, 3);

const sai_attr_metadata_t sai_mirror_attr_metadata[] = {

    {
        .objecttype             = SAI_OBJECT_TYPE_MIRROR,
        .attrid                 = SAI_MIRROR_SESSION_ATTR_TYPE,
        .attridname             = "SAI_MIRROR_SESSION_ATTR_TYPE",
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = StringifyEnum ( sai_mirror_type_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_mirror_type_t ),
        .enummetadata           = &metadata_enum_sai_mirror_type_t,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_MIRROR,
        .attrid                 = SAI_MIRROR_SESSION_ATTR_MONITOR_PORT,
        .attridname             = "SAI_MIRROR_SESSION_ATTR_MONITOR_PORT",
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_PORT },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    /*{
        .objecttype             = SAI_OBJECT_TYPE_MIRROR,
        .attrid                 = SAI_MIRROR_SESSION_ATTR_TRUNCATE_SIZE,
        .attridname             = "SAI_MIRROR_SESSION_ATTR_TRUNCATE_SIZE",
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT16,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .u16 = 0 },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },*/

    {
        .objecttype             = SAI_OBJECT_TYPE_MIRROR,
        .attrid                 = SAI_MIRROR_SESSION_ATTR_TC,
        .attridname             = "SAI_MIRROR_SESSION_ATTR_TC",
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT8,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .u8 = 0 },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    // TODO below attributes valid only when RSPAN or ERSPAN

    {
        .objecttype             = SAI_OBJECT_TYPE_MIRROR,
        .attrid                 = SAI_MIRROR_SESSION_ATTR_VLAN_TPID,
        .attridname             = "SAI_MIRROR_SESSION_ATTR_VLAN_TPID",
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT16,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = {
                                        COND_ENUM ( SAI_MIRROR_SESSION_ATTR_TYPE, SAI_MIRROR_TYPE_ENHANCED_REMOTE ),
                                        COND_ENUM ( SAI_MIRROR_SESSION_ATTR_TYPE, SAI_MIRROR_TYPE_REMOTE),
                                  },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_MIRROR,
        .attrid                 = SAI_MIRROR_SESSION_ATTR_VLAN_ID,
        .attridname             = "SAI_MIRROR_SESSION_ATTR_VLAN_ID",
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT16,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = {
                                        COND_ENUM ( SAI_MIRROR_SESSION_ATTR_TYPE, SAI_MIRROR_TYPE_ENHANCED_REMOTE ),
                                        COND_ENUM ( SAI_MIRROR_SESSION_ATTR_TYPE, SAI_MIRROR_TYPE_REMOTE),
                                  },

        // TODO extra logic will be required to check if vlan exists
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_MIRROR,
        .attrid                 = SAI_MIRROR_SESSION_ATTR_VLAN_PRI,
        .attridname             = "SAI_MIRROR_SESSION_ATTR_VLAN_PRI",
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT8,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = {
                                        COND_ENUM ( SAI_MIRROR_SESSION_ATTR_TYPE, SAI_MIRROR_TYPE_ENHANCED_REMOTE ),
                                        COND_ENUM ( SAI_MIRROR_SESSION_ATTR_TYPE, SAI_MIRROR_TYPE_REMOTE),
                                  },

        // TODO extra logic will be required to check if vlan exists
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_MIRROR,
        .attrid                 = SAI_MIRROR_SESSION_ATTR_VLAN_CFI,
        .attridname             = "SAI_MIRROR_SESSION_ATTR_VLAN_CFI",
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT8,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = {
                                        COND_ENUM ( SAI_MIRROR_SESSION_ATTR_TYPE, SAI_MIRROR_TYPE_ENHANCED_REMOTE ),
                                        COND_ENUM ( SAI_MIRROR_SESSION_ATTR_TYPE, SAI_MIRROR_TYPE_REMOTE),
                                  },
    },

    // TODO valid only for ERSPAN

    {
        .objecttype             = SAI_OBJECT_TYPE_MIRROR,
        .attrid                 = SAI_MIRROR_SESSION_ATTR_ENCAP_TYPE,
        .attridname             = "SAI_MIRROR_SESSION_ATTR_ENCAP_TYPE",
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = StringifyEnum ( sai_erspan_encapsulation_type_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_erspan_encapsulation_type_t ),
        .enummetadata           = &metadata_enum_sai_erspan_encapsulation_type_t,
        .conditions             = { COND_ENUM ( SAI_MIRROR_SESSION_ATTR_TYPE, SAI_MIRROR_TYPE_ENHANCED_REMOTE ) },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_MIRROR,
        .attrid                 = SAI_MIRROR_SESSION_ATTR_IPHDR_VERSION,
        .attridname             = "SAI_MIRROR_SESSION_ATTR_IPHDR_VERSION",
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT8,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { COND_ENUM ( SAI_MIRROR_SESSION_ATTR_TYPE, SAI_MIRROR_TYPE_ENHANCED_REMOTE ) },

        // TODO extra logic to check version
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_MIRROR,
        .attrid                 = SAI_MIRROR_SESSION_ATTR_TOS,
        .attridname             = "SAI_MIRROR_SESSION_ATTR_TOS",
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT8,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { COND_ENUM ( SAI_MIRROR_SESSION_ATTR_TYPE, SAI_MIRROR_TYPE_ENHANCED_REMOTE ) },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_MIRROR,
        .attrid                 = SAI_MIRROR_SESSION_ATTR_TTL,
        .attridname             = "SAI_MIRROR_SESSION_ATTR_TTL",
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT8,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .u8 = 255 },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },

        // TODO valid only when
        // .conditions             = { COND_ENUM ( SAI_MIRROR_SESSION_ATTR_TYPE, SAI_MIRROR_TYPE_ENHANCED_REMOTE ) },
    },

    // TODO what happen when we change IP header version 4 vs 6 and
    // don't change src/dst ip version?

    {
        .objecttype             = SAI_OBJECT_TYPE_MIRROR,
        .attrid                 = SAI_MIRROR_SESSION_ATTR_SRC_IP_ADDRESS,
        .attridname             = "SAI_MIRROR_SESSION_ATTR_SRC_IP_ADDRESS",
        .serializationtype      = SAI_SERIALIZATION_TYPE_IP_ADDRESS,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { COND_ENUM ( SAI_MIRROR_SESSION_ATTR_TYPE, SAI_MIRROR_TYPE_ENHANCED_REMOTE ) },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_MIRROR,
        .attrid                 = SAI_MIRROR_SESSION_ATTR_DST_IP_ADDRESS,
        .attridname             = "SAI_MIRROR_SESSION_ATTR_DST_IP_ADDRESS",
        .serializationtype      = SAI_SERIALIZATION_TYPE_IP_ADDRESS,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { COND_ENUM ( SAI_MIRROR_SESSION_ATTR_TYPE, SAI_MIRROR_TYPE_ENHANCED_REMOTE ) },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_MIRROR,
        .attrid                 = SAI_MIRROR_SESSION_ATTR_SRC_MAC_ADDRESS,
        .attridname             = "SAI_MIRROR_SESSION_ATTR_SRC_MAC_ADDRESS",
        .serializationtype      = SAI_SERIALIZATION_TYPE_MAC,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { COND_ENUM ( SAI_MIRROR_SESSION_ATTR_TYPE, SAI_MIRROR_TYPE_ENHANCED_REMOTE ) },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_MIRROR,
        .attrid                 = SAI_MIRROR_SESSION_ATTR_DST_MAC_ADDRESS,
        .attridname             = "SAI_MIRROR_SESSION_ATTR_DST_MAC_ADDRESS",
        .serializationtype      = SAI_SERIALIZATION_TYPE_MAC,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { COND_ENUM ( SAI_MIRROR_SESSION_ATTR_TYPE, SAI_MIRROR_TYPE_ENHANCED_REMOTE ) },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_MIRROR,
        .attrid                 = SAI_MIRROR_SESSION_ATTR_GRE_PROTOCOL_TYPE,
        .attridname             = "SAI_MIRROR_SESSION_ATTR_GRE_PROTOCOL_TYPE",
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT16,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { COND_ENUM ( SAI_MIRROR_SESSION_ATTR_TYPE, SAI_MIRROR_TYPE_ENHANCED_REMOTE ) },

        // TODO extra logic for checking value may be needed
    },

};

const size_t sai_mirror_attr_metadata_count = sizeof(sai_mirror_attr_metadata)/sizeof(sai_attr_metadata_t);
