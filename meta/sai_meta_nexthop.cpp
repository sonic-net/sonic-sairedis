#include "sai_meta.h"

// METADATA for SAI_OBJECT_TYPE_NEXT_HOP

DEFINE_ENUM_VALUES(sai_next_hop_type_t)
{
    SAI_NEXT_HOP_IP,
    SAI_NEXT_HOP_TUNNEL_ENCAP,
    // SAI_NEXT_HOP_MPLS, // not supported yet
};

const sai_attr_metadata_t sai_nexthop_attr_metadata[] = {

    {
        .objecttype             = SAI_OBJECT_TYPE_NEXT_HOP,
        .attrid                 = SAI_NEXT_HOP_ATTR_TYPE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = StringifyEnum ( sai_next_hop_type_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_next_hop_type_t ),
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_NEXT_HOP,
        .attrid                 = SAI_NEXT_HOP_ATTR_IP,
        .serializationtype      = SAI_SERIALIZATION_TYPE_IP_ADDRESS,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { COND_ENUM ( SAI_NEXT_HOP_ATTR_TYPE, SAI_NEXT_HOP_IP ) },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_NEXT_HOP,
        .attrid                 = SAI_NEXT_HOP_ATTR_ROUTER_INTERFACE_ID,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_ROUTER_INTERFACE },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_NEXT_HOP,
        .attrid                 = SAI_NEXT_HOP_ATTR_TUNNEL_ID,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_TUNNEL },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { COND_ENUM ( SAI_NEXT_HOP_ATTR_TYPE, SAI_NEXT_HOP_TUNNEL_ENCAP ) },
    },
};

const size_t sai_nexthop_attr_metadata_count = sizeof(sai_nexthop_attr_metadata)/sizeof(sai_attr_metadata_t);
