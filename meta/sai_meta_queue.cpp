#include "sai_meta.h"

// METADATA for SAI_OBJECT_TYPE_QUEUE

#ifndef SAI_QUEUE_ATTR_INDEX
#define SAI_QUEUE_ATTR_INDEX 0x100 // TODO remove on new SAI
#endif

DEFINE_ENUM_VALUES(sai_queue_type_t)
{
    SAI_QUEUE_TYPE_ALL,
    SAI_QUEUE_TYPE_UNICAST,
    SAI_QUEUE_TYPE_MULTICAST
};

const char metadata_sai_queue_type_t_enum_name[] = "sai_queue_type_t";
const sai_queue_type_t metadata_sai_queue_type_t_enum_values[] = {
    SAI_QUEUE_TYPE_ALL,
    SAI_QUEUE_TYPE_UNICAST,
    SAI_QUEUE_TYPE_MULTICAST,
};
const char* metadata_sai_queue_type_t_enum_values_names[] = {
    "SAI_QUEUE_TYPE_ALL",
    "SAI_QUEUE_TYPE_UNICAST",
    "SAI_QUEUE_TYPE_MULTICAST",
    NULL
};
const char* metadata_sai_queue_type_t_enum_values_short_names[] = {
    "ALL",
    "UNICAST",
    "MULTICAST",
    NULL
};
const size_t metadata_sai_queue_type_t_enum_values_count = 3;
DEFINE_ENUM_METADATA(sai_queue_type_t, 3);

const sai_attr_metadata_t sai_queue_attr_metadata[] = {

    {
        .objecttype             = SAI_OBJECT_TYPE_QUEUE,
        .attrid                 = SAI_QUEUE_ATTR_TYPE,
        .attridname             = "SAI_QUEUE_ATTR_TYPE",
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY | SAI_ATTR_FLAGS_KEY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = StringifyEnum ( sai_queue_type_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_queue_type_t ),
        .enummetadata           = &metadata_enum_sai_queue_type_t,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_QUEUE,
        .attrid                 = SAI_QUEUE_ATTR_INDEX,
        .attridname             = "SAI_QUEUE_ATTR_INDEX",
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT8,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY | SAI_ATTR_FLAGS_KEY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,

        .conditions             = { },

        // TODO extra validation may be needed on queue index
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_QUEUE,
        .attrid                 = SAI_QUEUE_ATTR_WRED_PROFILE_ID,
        .attridname             = "SAI_QUEUE_ATTR_WRED_PROFILE_ID",
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_WRED },
        .allownullobjectid      = true,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .oid = SAI_NULL_OBJECT_ID }, // there may be default wred
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_QUEUE,
        .attrid                 = SAI_QUEUE_ATTR_BUFFER_PROFILE_ID,
        .attridname             = "SAI_QUEUE_ATTR_BUFFER_PROFILE_ID",
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_BUFFER_PROFILE },
        .allownullobjectid      = true,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .oid = SAI_NULL_OBJECT_ID }, // there may be default wred
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_QUEUE,
        .attrid                 = SAI_QUEUE_ATTR_SCHEDULER_PROFILE_ID,
        .attridname             = "SAI_QUEUE_ATTR_SCHEDULER_PROFILE_ID",
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_SCHEDULER },
        .allownullobjectid      = true,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .oid = SAI_NULL_OBJECT_ID }, // there may be default wred
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },
};

const size_t sai_queue_attr_metadata_count = sizeof(sai_queue_attr_metadata)/sizeof(sai_attr_metadata_t);
