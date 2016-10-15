#include "sai_meta.h"

// METADATA for SAI_OBJECT_TYPE_LAG

const sai_attr_metadata_t sai_lag_attr_metadata[] = {

    {
        .objecttype             = SAI_OBJECT_TYPE_LAG,
        .attrid                 = SAI_LAG_ATTR_PORT_LIST,
        .attridname             = "SAI_LAG_ATTR_PORT_LIST",
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_LIST,
        .flags                  = SAI_ATTR_FLAGS_READ_ONLY,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_LAG_MEMBER },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },
};

const size_t sai_lag_attr_metadata_count = sizeof(sai_lag_attr_metadata)/sizeof(sai_attr_metadata_t);

// METADATA for SAI_OBJECT_TYPE_LAG_MEMBER

const sai_attr_metadata_t sai_lag_member_attr_metadata[] = {

    {
        .objecttype             = SAI_OBJECT_TYPE_LAG_MEMBER,
        .attrid                 = SAI_LAG_MEMBER_ATTR_LAG_ID,
        .attridname             = "SAI_LAG_MEMBER_ATTR_LAG_ID",
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_LAG },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_LAG_MEMBER,
        .attrid                 = SAI_LAG_MEMBER_ATTR_PORT_ID,
        .attridname             = "SAI_LAG_MEMBER_ATTR_PORT_ID",
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_PORT },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_LAG_MEMBER,
        .attrid                 = SAI_LAG_MEMBER_ATTR_EGRESS_DISABLE,
        .attridname             = "SAI_LAG_MEMBER_ATTR_EGRESS_DISABLE",
        .serializationtype      = SAI_SERIALIZATION_TYPE_BOOL,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
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
        .objecttype             = SAI_OBJECT_TYPE_LAG_MEMBER,
        .attrid                 = SAI_LAG_MEMBER_ATTR_INGRESS_DISABLE,
        .attridname             = "SAI_LAG_MEMBER_ATTR_INGRESS_DISABLE",
        .serializationtype      = SAI_SERIALIZATION_TYPE_BOOL,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .booldata = false },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .enummetadata           = NULL,
        .conditions             = { },
    },
};

const size_t sai_lag_member_attr_metadata_count = sizeof(sai_lag_member_attr_metadata)/sizeof(sai_attr_metadata_t);
