#include "sai_meta.h"

// METADATA for SAI_OBJECT_TYPE_WRED

DEFINE_ENUM_VALUES(sai_ecn_mark_mode_t)
{
    SAI_ECN_MARK_MODE_NONE,
    SAI_ECN_MARK_MODE_GREEN,
    SAI_ECN_MARK_MODE_YELLOW,
    SAI_ECN_MARK_MODE_RED,
    SAI_ECN_MARK_MODE_GREEN_YELLOW,
    SAI_ECN_MARK_MODE_GREEN_RED,
    SAI_ECN_MARK_MODE_YELLOW_RED,
    SAI_ECN_MARK_MODE_ALL
};

const char metadata_sai_ecn_mark_mode_t_enum_name[] = "sai_ecn_mark_mode_t";
const sai_ecn_mark_mode_t metadata_sai_ecn_mark_mode_t_enum_values[] = {
    SAI_ECN_MARK_MODE_NONE,
    SAI_ECN_MARK_MODE_GREEN,
    SAI_ECN_MARK_MODE_YELLOW,
    SAI_ECN_MARK_MODE_RED,
    SAI_ECN_MARK_MODE_GREEN_YELLOW,
    SAI_ECN_MARK_MODE_GREEN_RED,
    SAI_ECN_MARK_MODE_YELLOW_RED,
    SAI_ECN_MARK_MODE_ALL,
};
const char* metadata_sai_ecn_mark_mode_t_enum_values_names[] = {
    "SAI_ECN_MARK_MODE_NONE",
    "SAI_ECN_MARK_MODE_GREEN",
    "SAI_ECN_MARK_MODE_YELLOW",
    "SAI_ECN_MARK_MODE_RED",
    "SAI_ECN_MARK_MODE_GREEN_YELLOW",
    "SAI_ECN_MARK_MODE_GREEN_RED",
    "SAI_ECN_MARK_MODE_YELLOW_RED",
    "SAI_ECN_MARK_MODE_ALL",
    NULL
};
const char* metadata_sai_ecn_mark_mode_t_enum_values_short_names[] = {
    "NONE",
    "GREEN",
    "YELLOW",
    "RED",
    "GREEN_YELLOW",
    "GREEN_RED",
    "YELLOW_RED",
    "ALL",
    NULL
};
const size_t metadata_sai_ecn_mark_mode_t_enum_values_count = 8;
DEFINE_ENUM_METADATA(sai_ecn_mark_mode_t, 8);

const sai_attr_metadata_t sai_wred_attr_metadata[] = {

    {
        .objecttype             = SAI_OBJECT_TYPE_WRED,
        .attrid                 = SAI_WRED_ATTR_ECN_MARK_MODE,
        .attridname             = "SAI_WRED_ATTR_ECN_MARK_MODE",
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .s32 = SAI_ECN_MARK_MODE_NONE },
        .enumtypestr            = StringifyEnum ( sai_ecn_mark_mode_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_ecn_mark_mode_t ),
        .enummetadata           = &metadata_enum_sai_ecn_mark_mode_t,
        .conditions             = { },

        // NOTE: attribute is CREATE_ONLY since when type will change it may impact thresholds
    },

    // GREEN

    {
        .objecttype             = SAI_OBJECT_TYPE_WRED,
        .attrid                 = SAI_WRED_ATTR_GREEN_ENABLE,
        .attridname             = "SAI_WRED_ATTR_GREEN_ENABLE",
        .serializationtype      = SAI_SERIALIZATION_TYPE_BOOL,
        .flags                  = SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .booldata = false },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },

        // NOTE: attribute is CREATE_ONLY since when type will change it may impact thresholds
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_WRED,
        .attrid                 = SAI_WRED_ATTR_GREEN_MIN_THRESHOLD,
        .attridname             = "SAI_WRED_ATTR_GREEN_MIN_THRESHOLD",
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT32,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = {
                                        COND_BOOL ( SAI_WRED_ATTR_GREEN_ENABLE,  true ),
                                        COND_ENUM ( SAI_WRED_ATTR_ECN_MARK_MODE, SAI_ECN_MARK_MODE_GREEN ),
                                        COND_ENUM ( SAI_WRED_ATTR_ECN_MARK_MODE, SAI_ECN_MARK_MODE_GREEN_YELLOW ),
                                        COND_ENUM ( SAI_WRED_ATTR_ECN_MARK_MODE, SAI_ECN_MARK_MODE_GREEN_RED ),
                                        COND_ENUM ( SAI_WRED_ATTR_ECN_MARK_MODE, SAI_ECN_MARK_MODE_ALL ),
                                  },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_WRED,
        .attrid                 = SAI_WRED_ATTR_GREEN_MAX_THRESHOLD,
        .attridname             = "SAI_WRED_ATTR_GREEN_MAX_THRESHOLD",
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT32,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = {
                                        COND_BOOL ( SAI_WRED_ATTR_GREEN_ENABLE,  true ),
                                        COND_ENUM ( SAI_WRED_ATTR_ECN_MARK_MODE, SAI_ECN_MARK_MODE_GREEN ),
                                        COND_ENUM ( SAI_WRED_ATTR_ECN_MARK_MODE, SAI_ECN_MARK_MODE_GREEN_YELLOW ),
                                        COND_ENUM ( SAI_WRED_ATTR_ECN_MARK_MODE, SAI_ECN_MARK_MODE_GREEN_RED ),
                                        COND_ENUM ( SAI_WRED_ATTR_ECN_MARK_MODE, SAI_ECN_MARK_MODE_ALL ),
                                  },

        // TODO range check 1.. max
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_WRED,
        .attrid                 = SAI_WRED_ATTR_GREEN_DROP_PROBABILITY,
        .attridname             = "SAI_WRED_ATTR_GREEN_DROP_PROBABILITY",
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .u32 = 100 },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },


    // YELLOW

    {
        .objecttype             = SAI_OBJECT_TYPE_WRED,
        .attrid                 = SAI_WRED_ATTR_YELLOW_ENABLE,
        .attridname             = "SAI_WRED_ATTR_YELLOW_ENABLE",
        .serializationtype      = SAI_SERIALIZATION_TYPE_BOOL,
        .flags                  = SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .booldata = false },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_WRED,
        .attrid                 = SAI_WRED_ATTR_YELLOW_MIN_THRESHOLD,
        .attridname             = "SAI_WRED_ATTR_YELLOW_MIN_THRESHOLD",
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT32,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = {
                                        COND_BOOL ( SAI_WRED_ATTR_YELLOW_ENABLE, true ),
                                        COND_ENUM ( SAI_WRED_ATTR_ECN_MARK_MODE, SAI_ECN_MARK_MODE_YELLOW ),
                                        COND_ENUM ( SAI_WRED_ATTR_ECN_MARK_MODE, SAI_ECN_MARK_MODE_GREEN_YELLOW ),
                                        COND_ENUM ( SAI_WRED_ATTR_ECN_MARK_MODE, SAI_ECN_MARK_MODE_YELLOW_RED ),
                                        COND_ENUM ( SAI_WRED_ATTR_ECN_MARK_MODE, SAI_ECN_MARK_MODE_ALL ),
                                  },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_WRED,
        .attrid                 = SAI_WRED_ATTR_YELLOW_MAX_THRESHOLD,
        .attridname             = "SAI_WRED_ATTR_YELLOW_MAX_THRESHOLD",
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT32,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = {
                                        COND_BOOL ( SAI_WRED_ATTR_YELLOW_ENABLE, true ),
                                        COND_ENUM ( SAI_WRED_ATTR_ECN_MARK_MODE, SAI_ECN_MARK_MODE_YELLOW ),
                                        COND_ENUM ( SAI_WRED_ATTR_ECN_MARK_MODE, SAI_ECN_MARK_MODE_GREEN_YELLOW ),
                                        COND_ENUM ( SAI_WRED_ATTR_ECN_MARK_MODE, SAI_ECN_MARK_MODE_YELLOW_RED ),
                                        COND_ENUM ( SAI_WRED_ATTR_ECN_MARK_MODE, SAI_ECN_MARK_MODE_ALL ),
                                  },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_WRED,
        .attrid                 = SAI_WRED_ATTR_YELLOW_DROP_PROBABILITY,
        .attridname             = "SAI_WRED_ATTR_YELLOW_DROP_PROBABILITY",
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .u32 = 100 },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    // RED

    {
        .objecttype             = SAI_OBJECT_TYPE_WRED,
        .attrid                 = SAI_WRED_ATTR_RED_ENABLE,
        .attridname             = "SAI_WRED_ATTR_RED_ENABLE",
        .serializationtype      = SAI_SERIALIZATION_TYPE_BOOL,
        .flags                  = SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .booldata = false },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_WRED,
        .attrid                 = SAI_WRED_ATTR_RED_MIN_THRESHOLD,
        .attridname             = "SAI_WRED_ATTR_RED_MIN_THRESHOLD",
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT32,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = {
                                        COND_BOOL ( SAI_WRED_ATTR_RED_ENABLE,    true ),
                                        COND_ENUM ( SAI_WRED_ATTR_ECN_MARK_MODE, SAI_ECN_MARK_MODE_RED ),
                                        COND_ENUM ( SAI_WRED_ATTR_ECN_MARK_MODE, SAI_ECN_MARK_MODE_GREEN_RED ),
                                        COND_ENUM ( SAI_WRED_ATTR_ECN_MARK_MODE, SAI_ECN_MARK_MODE_YELLOW_RED ),
                                        COND_ENUM ( SAI_WRED_ATTR_ECN_MARK_MODE, SAI_ECN_MARK_MODE_ALL ),
                                  },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_WRED,
        .attrid                 = SAI_WRED_ATTR_RED_MAX_THRESHOLD,
        .attridname             = "SAI_WRED_ATTR_RED_MAX_THRESHOLD",
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT32,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = {
                                        COND_BOOL ( SAI_WRED_ATTR_RED_ENABLE,    true ),
                                        COND_ENUM ( SAI_WRED_ATTR_ECN_MARK_MODE, SAI_ECN_MARK_MODE_RED ),
                                        COND_ENUM ( SAI_WRED_ATTR_ECN_MARK_MODE, SAI_ECN_MARK_MODE_GREEN_RED ),
                                        COND_ENUM ( SAI_WRED_ATTR_ECN_MARK_MODE, SAI_ECN_MARK_MODE_YELLOW_RED ),
                                        COND_ENUM ( SAI_WRED_ATTR_ECN_MARK_MODE, SAI_ECN_MARK_MODE_ALL ),
                                  },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_WRED,
        .attrid                 = SAI_WRED_ATTR_RED_DROP_PROBABILITY,
        .attridname             = "SAI_WRED_ATTR_RED_DROP_PROBABILITY",
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .u32 = 100 },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    // RED END

    {
        .objecttype             = SAI_OBJECT_TYPE_WRED,
        .attrid                 = SAI_WRED_ATTR_WEIGHT,
        .attridname             = "SAI_WRED_ATTR_WEIGHT",
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

        // TODO validte range 0 .. 15
    },
};

const size_t sai_wred_attr_metadata_count = sizeof(sai_wred_attr_metadata)/sizeof(sai_attr_metadata_t);
