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

const char metadata_sai_policer_color_source_t_enum_name[] = "sai_policer_color_source_t";
const sai_policer_color_source_t metadata_sai_policer_color_source_t_enum_values[] = {
    SAI_POLICER_COLOR_SOURCE_BLIND,
    SAI_POLICER_COLOR_SOURCE_AWARE,
};
const char* metadata_sai_policer_color_source_t_enum_values_names[] = {
    "SAI_POLICER_COLOR_SOURCE_BLIND",
    "SAI_POLICER_COLOR_SOURCE_AWARE",
    NULL
};
const char* metadata_sai_policer_color_source_t_enum_values_short_names[] = {
    "BLIND",
    "AWARE",
    NULL
};
const size_t metadata_sai_policer_color_source_t_enum_values_count = 2;
DEFINE_ENUM_METADATA(sai_policer_color_source_t, 2);

const char metadata_sai_policer_mode_t_enum_name[] = "sai_policer_mode_t";
const sai_policer_mode_t metadata_sai_policer_mode_t_enum_values[] = {
    SAI_POLICER_MODE_Sr_TCM,
    SAI_POLICER_MODE_Tr_TCM,
    SAI_POLICER_MODE_STORM_CONTROL,
};
const char* metadata_sai_policer_mode_t_enum_values_names[] = {
    "SAI_POLICER_MODE_SR_TCM",
    "SAI_POLICER_MODE_TR_TCM",
    "SAI_POLICER_MODE_STORM_CONTROL",
    NULL
};
const char* metadata_sai_policer_mode_t_enum_values_short_names[] = {
    "SR_TCM",
    "TR_TCM",
    "STORM_CONTROL",
    NULL
};
const size_t metadata_sai_policer_mode_t_enum_values_count = 3;
DEFINE_ENUM_METADATA(sai_policer_mode_t, 3);

const char metadata_sai_meter_type_t_enum_name[] = "sai_meter_type_t";
const sai_meter_type_t metadata_sai_meter_type_t_enum_values[] = {
    SAI_METER_TYPE_PACKETS,
    SAI_METER_TYPE_BYTES,
};
const char* metadata_sai_meter_type_t_enum_values_names[] = {
    "SAI_METER_TYPE_PACKETS",
    "SAI_METER_TYPE_BYTES",
    NULL
};
const char* metadata_sai_meter_type_t_enum_values_short_names[] = {
    "PACKETS",
    "BYTES",
    NULL
};
const size_t metadata_sai_meter_type_t_enum_values_count = 2;
DEFINE_ENUM_METADATA(sai_meter_type_t, 2);

const sai_attr_metadata_t sai_policer_attr_metadata[] = {

    {
        .objecttype             = SAI_OBJECT_TYPE_POLICER,
        .attrid                 = SAI_POLICER_ATTR_METER_TYPE,
        .attridname             = "SAI_POLICER_ATTR_METER_TYPE",
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = StringifyEnum ( sai_meter_type_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_meter_type_t ),
        .enummetadata           = &metadata_enum_sai_meter_type_t,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_POLICER,
        .attrid                 = SAI_POLICER_ATTR_MODE,
        .attridname             = "SAI_POLICER_ATTR_MODE",
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = StringifyEnum ( sai_policer_mode_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_policer_mode_t ),
        .enummetadata           = &metadata_enum_sai_policer_mode_t,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_POLICER,
        .attrid                 = SAI_POLICER_ATTR_COLOR_SOURCE,
        .attridname             = "SAI_POLICER_ATTR_COLOR_SOURCE",
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .s32 = SAI_POLICER_COLOR_SOURCE_AWARE },
        .enumtypestr            = StringifyEnum ( sai_policer_color_source_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_policer_color_source_t ),
        .enummetadata           = &metadata_enum_sai_policer_color_source_t,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_POLICER,
        .attrid                 = SAI_POLICER_ATTR_CBS,
        .attridname             = "SAI_POLICER_ATTR_CBS",
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT64,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .u64 = 0 },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_POLICER,
        .attrid                 = SAI_POLICER_ATTR_CIR,
        .attridname             = "SAI_POLICER_ATTR_CIR",
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT64,
        .flags                  = SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .u64 = 0 },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_POLICER,
        .attrid                 = SAI_POLICER_ATTR_PBS,
        .attridname             = "SAI_POLICER_ATTR_PBS",
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT64,
        .flags                  = SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .u64 = 0 },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_POLICER,
        .attrid                 = SAI_POLICER_ATTR_PIR,
        .attridname             = "SAI_POLICER_ATTR_PIR",
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT64,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { COND_ENUM ( SAI_POLICER_ATTR_METER_TYPE, SAI_POLICER_MODE_Tr_TCM ) },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_POLICER,
        .attrid                 = SAI_POLICER_ATTR_GREEN_PACKET_ACTION,
        .attridname             = "SAI_POLICER_ATTR_GREEN_PACKET_ACTION",
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .s32 = SAI_PACKET_ACTION_FORWARD },
        .enumtypestr            = StringifyEnum ( sai_packet_action_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_packet_action_t ),
        .enummetadata           = &metadata_enum_sai_packet_action_t,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_POLICER,
        .attrid                 = SAI_POLICER_ATTR_YELLOW_PACKET_ACTION,
        .attridname             = "SAI_POLICER_ATTR_YELLOW_PACKET_ACTION",
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .s32 = SAI_PACKET_ACTION_FORWARD },
        .enumtypestr            = StringifyEnum ( sai_packet_action_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_packet_action_t ),
        .enummetadata           = &metadata_enum_sai_packet_action_t,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_POLICER,
        .attrid                 = SAI_POLICER_ATTR_RED_PACKET_ACTION,
        .attridname             = "SAI_POLICER_ATTR_RED_PACKET_ACTION",
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .s32 = SAI_PACKET_ACTION_FORWARD },
        .enumtypestr            = StringifyEnum ( sai_packet_action_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_packet_action_t ),
        .enummetadata           = &metadata_enum_sai_packet_action_t,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_POLICER,
        .attrid                 = SAI_POLICER_ATTR_ENABLE_COUNTER_LIST,
        .attridname             = "SAI_POLICER_ATTR_ENABLE_COUNTER_LIST",
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32_LIST,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_EMPTY_LIST,
        .defaultvalue           = { },
        .enumtypestr            = StringifyEnum ( sai_packet_action_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_packet_action_t ),
        .enummetadata           = &metadata_enum_sai_packet_action_t,
        .conditions             = { },
    },
};

const size_t sai_policer_attr_metadata_count = sizeof(sai_policer_attr_metadata)/sizeof(sai_attr_metadata_t);
