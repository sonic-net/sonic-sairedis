#include "sai_meta.h"

// METADATA for SAI_OBJECT_TYPE_SCHEDULER

DEFINE_ENUM_VALUES(sai_scheduling_type_t)
{
    SAI_SCHEDULING_STRICT,
    SAI_SCHEDULING_WRR,
    SAI_SCHEDULING_DWRR
};

const sai_attr_metadata_t sai_scheduler_attr_metadata[] = {

    {
        .objecttype             = SAI_OBJECT_TYPE_SCHEDULER,
        .attrid                 = SAI_SCHEDULER_ATTR_SCHEDULING_ALGORITHM,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .s32 = SAI_SCHEDULING_WRR },
        .enumtypestr            = StringifyEnum ( sai_scheduling_type_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_scheduling_type_t ),
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_SCHEDULER,
        .attrid                 = SAI_SCHEDULER_ATTR_SCHEDULING_WEIGHT,
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT8,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .u8 = 1 },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },

        // TODO valid only when SAI_SCHEDULER_ATTR_SCHEDULING_ALGORITHM = SAI_SCHEDULING_DWRR
        // TODO need to check range Range [1 - 100]
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_SCHEDULER,
        .attrid                 = SAI_SCHEDULER_ATTR_SHAPER_TYPE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .s32 = SAI_METER_TYPE_BYTES },
        .enumtypestr            = StringifyEnum ( sai_meter_type_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_meter_type_t ),
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_SCHEDULER,
        .attrid                 = SAI_SCHEDULER_ATTR_MIN_BANDWIDTH_RATE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT64,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .u64 = 0 },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_SCHEDULER,
        .attrid                 = SAI_SCHEDULER_ATTR_MIN_BANDWIDTH_BURST_RATE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT64,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .u64 = 0 },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_SCHEDULER,
        .attrid                 = SAI_SCHEDULER_ATTR_MAX_BANDWIDTH_RATE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT64,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .u64 = 0 },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_SCHEDULER,
        .attrid                 = SAI_SCHEDULER_ATTR_MAX_BANDWIDTH_BURST_RATE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT64,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .u64 = 0 },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },
};

const size_t sai_scheduler_attr_metadata_count = sizeof(sai_scheduler_attr_metadata)/sizeof(sai_attr_metadata_t);
