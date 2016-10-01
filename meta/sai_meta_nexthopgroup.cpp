#include "sai_meta.h"

// METADATA for SAI_OBJECT_TYPE_NEXT_HOP_GROUP

DEFINE_ENUM_VALUES(sai_next_hop_group_type_t)
{
    SAI_NEXT_HOP_GROUP_ECMP
};

const sai_attr_metadata_t sai_nexthopgroup_attr_metadata[] = {

    {
        .objecttype             = SAI_OBJECT_TYPE_NEXT_HOP_GROUP,
        .attrid                 = SAI_NEXT_HOP_GROUP_ATTR_NEXT_HOP_COUNT,
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT32,
        .flags                  = SAI_ATTR_FLAGS_READ_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_NEXT_HOP_GROUP,
        .attrid                 = SAI_NEXT_HOP_GROUP_ATTR_TYPE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = StringifyEnum ( sai_next_hop_group_type_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_next_hop_group_type_t ),
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_NEXT_HOP_GROUP,
        .attrid                 = SAI_NEXT_HOP_GROUP_ATTR_NEXT_HOP_LIST,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_LIST,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_NEXT_HOP },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },
};

const size_t sai_nexthopgroup_attr_metadata_count = sizeof(sai_nexthopgroup_attr_metadata)/sizeof(sai_attr_metadata_t);
