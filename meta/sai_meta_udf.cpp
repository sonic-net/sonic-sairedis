#include "sai_meta.h"

// METADATA for SAI_OBJECT_TYPE_UDF

uint32_t hash_mask_default_length = 2;
uint8_t hash_mask_default_list[] = { 0xFF, 0xFF };

const sai_attribute_value_t hash_mask_default_value = {
    .u8list = {
        .count = hash_mask_default_length,
        .list = hash_mask_default_list
    }
};

DEFINE_ENUM_VALUES(sai_udf_base_t)
{
    SAI_UDF_BASE_L2,
    SAI_UDF_BASE_L3,
    SAI_UDF_BASE_L4
};

DEFINE_ENUM_VALUES(sai_udf_group_type_t)
{
    SAI_UDF_GROUP_GENERIC,
    SAI_UDF_GROUP_HASH
};

const char metadata_sai_udf_group_type_t_enum_name[] = "sai_udf_group_type_t";
const sai_udf_group_type_t metadata_sai_udf_group_type_t_enum_values[] = {
    SAI_UDF_GROUP_GENERIC,
    SAI_UDF_GROUP_HASH,
};
const char* metadata_sai_udf_group_type_t_enum_values_names[] = {
    "SAI_UDF_GROUP_TYPE_GENERIC",
    "SAI_UDF_GROUP_TYPE_HASH",
    NULL
};
const char* metadata_sai_udf_group_type_t_enum_values_short_names[] = {
    "GENERIC",
    "HASH",
    NULL
};
const size_t metadata_sai_udf_group_type_t_enum_values_count = 2;
DEFINE_ENUM_METADATA(sai_udf_group_type_t, 2);

const char metadata_sai_udf_base_t_enum_name[] = "sai_udf_base_t";
const sai_udf_base_t metadata_sai_udf_base_t_enum_values[] = {
    SAI_UDF_BASE_L2,
    SAI_UDF_BASE_L3,
    SAI_UDF_BASE_L4,
};
const char* metadata_sai_udf_base_t_enum_values_names[] = {
    "SAI_UDF_BASE_L2",
    "SAI_UDF_BASE_L3",
    "SAI_UDF_BASE_L4",
    NULL
};
const char* metadata_sai_udf_base_t_enum_values_short_names[] = {
    "L2",
    "L3",
    "L4",
    NULL
};
const size_t metadata_sai_udf_base_t_enum_values_count = 3;
DEFINE_ENUM_METADATA(sai_udf_base_t, 3);

const sai_attr_metadata_t sai_udf_attr_metadata[] = {

    {
        .objecttype             = SAI_OBJECT_TYPE_UDF,
        .attrid                 = SAI_UDF_ATTR_MATCH_ID,
        .attridname             = "SAI_UDF_ATTR_MATCH_ID",
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_UDF_MATCH },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_UDF,
        .attrid                 = SAI_UDF_ATTR_GROUP_ID,
        .attridname             = "SAI_UDF_ATTR_GROUP_ID",
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_UDF_GROUP },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_UDF,
        .attrid                 = SAI_UDF_ATTR_BASE,
        .attridname             = "SAI_UDF_ATTR_BASE",
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .s32 = SAI_UDF_BASE_L2 },
        .enumtypestr            = StringifyEnum ( sai_udf_base_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_udf_base_t ),
        .enummetadata           = &metadata_enum_sai_udf_base_t,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_UDF,
        .attrid                 = SAI_UDF_ATTR_OFFSET,
        .attridname             = "SAI_UDF_ATTR_OFFSET",
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT16,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_UDF,
        .attrid                 = SAI_UDF_ATTR_HASH_MASK,
        .attridname             = "SAI_UDF_ATTR_HASH_MASK",
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT8_LIST,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = hash_mask_default_value,
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },

        // TODO extra logic is needed here to validate list length (create and set)
    },
};

const size_t sai_udf_attr_metadata_count = sizeof(sai_udf_attr_metadata)/sizeof(sai_attr_metadata_t);

// METADATA for SAI_OBJECT_TYPE_UDF_MATCH

const sai_attr_metadata_t sai_udf_match_attr_metadata[] = {

    {
        .objecttype             = SAI_OBJECT_TYPE_UDF_MATCH,
        .attrid                 = SAI_UDF_MATCH_ATTR_L2_TYPE,
        .attridname             = "SAI_UDF_MATCH_ATTR_L2_TYPE",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT16,
        .flags                  = SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { }, // default to none
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_UDF_MATCH,
        .attrid                 = SAI_UDF_MATCH_ATTR_L3_TYPE,
        .attridname             = "SAI_UDF_MATCH_ATTR_L3_TYPE",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT8,
        .flags                  = SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { }, // default to none
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_UDF_MATCH,
        .attrid                 = SAI_UDF_MATCH_ATTR_GRE_TYPE,
        .attridname             = "SAI_UDF_MATCH_ATTR_GRE_TYPE",
        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT16,
        .flags                  = SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { }, // default to none
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_UDF_MATCH,
        .attrid                 = SAI_UDF_MATCH_ATTR_PRIORITY,
        .attridname             = "SAI_UDF_MATCH_ATTR_PRIORITY",
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT8,
        .flags                  = SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .u8 = 0 },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },
};

const size_t sai_udf_match_attr_metadata_count = sizeof(sai_udf_match_attr_metadata)/sizeof(sai_attr_metadata_t);

// METADATA for SAI_OBJECT_TYPE_UDF_GROUP

const sai_attr_metadata_t sai_udf_group_attr_metadata[] = {

    {
        .objecttype             = SAI_OBJECT_TYPE_UDF_GROUP,
        .attrid                 = SAI_UDF_GROUP_ATTR_UDF_LIST,
        .attridname             = "SAI_UDF_GROUP_ATTR_UDF_LIST",
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_LIST,
        .flags                  = SAI_ATTR_FLAGS_READ_ONLY,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_UDF },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_UDF_GROUP,
        .attrid                 = SAI_UDF_GROUP_ATTR_TYPE,
        .attridname             = "SAI_UDF_GROUP_ATTR_TYPE",
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .s32 = SAI_UDF_GROUP_GENERIC },
        .enumtypestr            = StringifyEnum ( sai_udf_group_type_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_udf_group_type_t ),
        .enummetadata           = &metadata_enum_sai_udf_group_type_t,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_UDF_GROUP,
        .attrid                 = SAI_UDF_GROUP_ATTR_LENGTH,
        .attridname             = "SAI_UDF_GROUP_ATTR_LENGTH",
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT16,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },
};

const size_t sai_udf_group_attr_metadata_count = sizeof(sai_udf_group_attr_metadata)/sizeof(sai_attr_metadata_t);
