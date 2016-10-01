#include "sai_meta.h"

// METADATA for SAI_OBJECT_TYPE_POLICER

DEFINE_ENUM_VALUES(sai_meter_type_t)
{
    SAI_METER_TYPE_PACKETS,
    SAI_METER_TYPE_BYTES
};

DEFINE_ENUM_VALUES(sai_policer_mode_t)
{
    SAI_POLICER_MODE_Sr_TCM,
    SAI_POLICER_MODE_Tr_TCM,
    SAI_POLICER_MODE_STORM_CONTROL
};

DEFINE_ENUM_VALUES(sai_policer_color_source_t)
{
    SAI_POLICER_COLOR_SOURCE_BLIND,
    SAI_POLICER_COLOR_SOURCE_AWARE
};

const sai_attr_metadata_t sai_policer_attr_metadata[] = {

    {
        .objecttype             = SAI_OBJECT_TYPE_POLICER,
        .attrid                 = SAI_POLICER_ATTR_METER_TYPE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = StringifyEnum ( sai_meter_type_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_meter_type_t ),
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_POLICER,
        .attrid                 = SAI_POLICER_ATTR_MODE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = StringifyEnum ( sai_policer_mode_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_policer_mode_t ),
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_POLICER,
        .attrid                 = SAI_POLICER_ATTR_COLOR_SOURCE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .s32 = SAI_POLICER_COLOR_SOURCE_AWARE },
        .enumtypestr            = StringifyEnum ( sai_policer_color_source_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_policer_color_source_t ),
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_POLICER,
        .attrid                 = SAI_POLICER_ATTR_CBS,
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT64,
        .flags                  = SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .u64 = 0 },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_POLICER,
        .attrid                 = SAI_POLICER_ATTR_CIR,
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT64,
        .flags                  = SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .u64 = 0 },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_POLICER,
        .attrid                 = SAI_POLICER_ATTR_PBS,
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT64,
        .flags                  = SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .u64 = 0 },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_POLICER,
        .attrid                 = SAI_POLICER_ATTR_PIR,
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT64,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { COND_ENUM ( SAI_POLICER_ATTR_METER_TYPE, SAI_POLICER_MODE_Tr_TCM ) },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_POLICER,
        .attrid                 = SAI_POLICER_ATTR_GREEN_PACKET_ACTION,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .s32 = SAI_PACKET_ACTION_FORWARD },
        .enumtypestr            = StringifyEnum ( sai_packet_action_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_packet_action_t ),
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_POLICER,
        .attrid                 = SAI_POLICER_ATTR_YELLOW_PACKET_ACTION,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .s32 = SAI_PACKET_ACTION_FORWARD },
        .enumtypestr            = StringifyEnum ( sai_packet_action_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_packet_action_t ),
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_POLICER,
        .attrid                 = SAI_POLICER_ATTR_RED_PACKET_ACTION,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .s32 = SAI_PACKET_ACTION_FORWARD },
        .enumtypestr            = StringifyEnum ( sai_packet_action_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_packet_action_t ),
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_POLICER,
        .attrid                 = SAI_POLICER_ATTR_ENABLE_COUNTER_LIST,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32_LIST,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_EMPTY_LIST,
        .defaultvalue           = { },
        .enumtypestr            = StringifyEnum ( sai_packet_action_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_packet_action_t ),
        .conditions             = { },
    },
};

const size_t sai_policer_attr_metadata_count = sizeof(sai_policer_attr_metadata)/sizeof(sai_attr_metadata_t);
