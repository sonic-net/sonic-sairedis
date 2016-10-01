#include "sai_meta.h"

// METADATA for SAI_OBJECT_TYPE_SCHEDULER_GROUP

const sai_attr_metadata_t sai_scheduler_group_attr_metadata[] = {

    {
        .objecttype             = SAI_OBJECT_TYPE_SCHEDULER_GROUP,
        .attrid                 = SAI_SCHEDULER_GROUP_ATTR_CHILD_COUNT,
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
        .objecttype             = SAI_OBJECT_TYPE_SCHEDULER_GROUP,
        .attrid                 = SAI_SCHEDULER_GROUP_ATTR_CHILD_LIST,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_LIST,
        .flags                  = SAI_ATTR_FLAGS_READ_ONLY,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_SCHEDULER }, // TODO there may be different types like QUEUE or PORT here
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },

        // TODO extra check may be needed here
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_SCHEDULER_GROUP,
        .attrid                 = SAI_SCHEDULER_GROUP_ATTR_PORT_ID,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_PORT },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_SCHEDULER_GROUP,
        .attrid                 = SAI_SCHEDULER_GROUP_ATTR_LEVEL,
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT8,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },

        // TODO validate range 0..16
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_SCHEDULER_GROUP,
        .attrid                 = SAI_SCHEDULER_GROUP_ATTR_MAX_CHILDS,
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT8,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },

        // TODO validate range 0..64 ?
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_SCHEDULER_GROUP,
        .attrid                 = SAI_SCHEDULER_GROUP_ATTR_SCHEDULER_PROFILE_ID,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_SCHEDULER },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },

        // TODO SET ONLY, special, not supported yet, must change api design
    },
};

const size_t sai_scheduler_group_attr_metadata_count = sizeof(sai_scheduler_group_attr_metadata)/sizeof(sai_attr_metadata_t);
