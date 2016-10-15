#include "sai_meta.h"

// METADATA for SAI_OBJECT_TYPE_NEXT_HOP

DEFINE_ENUM_VALUES(sai_next_hop_type_t)
{
    SAI_NEXT_HOP_IP,
    SAI_NEXT_HOP_MPLS,
    SAI_NEXT_HOP_TUNNEL_ENCAP,
};

const char metadata_sai_next_hop_type_t_enum_name[] = "sai_next_hop_type_t";
const sai_next_hop_type_t metadata_sai_next_hop_type_t_enum_values[] = {
    SAI_NEXT_HOP_IP,
    SAI_NEXT_HOP_MPLS,
    SAI_NEXT_HOP_TUNNEL_ENCAP,
};
const char* metadata_sai_next_hop_type_t_enum_values_names[] = {
    "SAI_NEXT_HOP_TYPE_IP",
    "SAI_NEXT_HOP_TYPE_MPLS",
    "SAI_NEXT_HOP_TYPE_TUNNEL_ENCAP",
    NULL
};
const char* metadata_sai_next_hop_type_t_enum_values_short_names[] = {
    "IP",
    "MPLS",
    "TUNNEL_ENCAP",
    NULL
};
const size_t metadata_sai_next_hop_type_t_enum_values_count = 3;
DEFINE_ENUM_METADATA(sai_next_hop_type_t, 3);

const sai_attr_metadata_t sai_nexthop_attr_metadata[] = {

    {
        .objecttype             = SAI_OBJECT_TYPE_NEXT_HOP,
        .attrid                 = SAI_NEXT_HOP_ATTR_TYPE,
        .attridname             = "SAI_NEXT_HOP_ATTR_TYPE",
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = StringifyEnum ( sai_next_hop_type_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_next_hop_type_t ),
        .enummetadata           = &metadata_enum_sai_next_hop_type_t,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_NEXT_HOP,
        .attrid                 = SAI_NEXT_HOP_ATTR_IP,
        .attridname             = "SAI_NEXT_HOP_ATTR_IP",
        .serializationtype      = SAI_SERIALIZATION_TYPE_IP_ADDRESS,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { COND_ENUM ( SAI_NEXT_HOP_ATTR_TYPE, SAI_NEXT_HOP_IP ) },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_NEXT_HOP,
        .attrid                 = SAI_NEXT_HOP_ATTR_ROUTER_INTERFACE_ID,
        .attridname             = "SAI_NEXT_HOP_ATTR_ROUTER_INTERFACE_ID",
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_ROUTER_INTERFACE },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_NEXT_HOP,
        .attrid                 = SAI_NEXT_HOP_ATTR_TUNNEL_ID,
        .attridname             = "SAI_NEXT_HOP_ATTR_TUNNEL_ID",
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_TUNNEL },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { COND_ENUM ( SAI_NEXT_HOP_ATTR_TYPE, SAI_NEXT_HOP_TUNNEL_ENCAP ) },
    },
};

const size_t sai_nexthop_attr_metadata_count = sizeof(sai_nexthop_attr_metadata)/sizeof(sai_attr_metadata_t);
