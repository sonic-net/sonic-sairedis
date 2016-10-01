#include "sai_meta.h"

// METADATA for SAI_OBJECT_TYPE_SAMPLEPACKET

DEFINE_ENUM_VALUES(sai_samplepacket_type_t)
{
    SAI_SAMPLEPACKET_SLOW_PATH
};

DEFINE_ENUM_VALUES(sai_samplepacket_mode_t)
{
    //SAI_SAMPLEPACKET_MODE_EXCLUSIVE,
    //SAI_SAMPLEPACKET_MODE_SHARED
};

const sai_attr_metadata_t sai_samplepacket_attr_metadata[] = {

    {
        .objecttype             = SAI_OBJECT_TYPE_SAMPLEPACKET,
        .attrid                 = SAI_SAMPLEPACKET_ATTR_SAMPLE_RATE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT32,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_SAMPLEPACKET,
        .attrid                 = SAI_SAMPLEPACKET_ATTR_TYPE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .s32 = SAI_SAMPLEPACKET_SLOW_PATH },
        .enumtypestr            = StringifyEnum ( sai_samplepacket_type_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_samplepacket_type_t ),
        .conditions             = { },
    },

    /*{
        .objecttype             = SAI_OBJECT_TYPE_SAMPLEPACKET,
        .attrid                 = SAI_SAMPLEPACKET_ATTR_MODE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .s32 = SAI_SAMPLEPACKET_MODE_EXCLUSIVE },
        .enumtypestr            = StringifyEnum ( sai_samplepacket_mode_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_samplepacket_mode_t ),
        .conditions             = { },
    },*/
};

const size_t sai_samplepacket_attr_metadata_count = sizeof(sai_samplepacket_attr_metadata)/sizeof(sai_attr_metadata_t);
