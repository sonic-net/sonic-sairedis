#include "sai_meta.h"

// METADATA for SAI_OBJECT_TYPE_PRIORITY_GROUP

const sai_attr_metadata_t sai_priority_group_attr_metadata[] = {

    {
        .objecttype             = SAI_OBJECT_TYPE_PRIORITY_GROUP,
        .attrid                 = SAI_INGRESS_PRIORITY_GROUP_ATTR_BUFFER_PROFILE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_BUFFER_PROFILE },
        .allownullobjectid      = true,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .oid = SAI_NULL_OBJECT_ID },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },
};

const size_t sai_priority_group_attr_metadata_count = sizeof(sai_priority_group_attr_metadata)/sizeof(sai_attr_metadata_t);

// METADATA for SAI_OBJECT_TYPE_BUFFER_POOL

DEFINE_ENUM_VALUES(sai_buffer_pool_type_t)
{
    SAI_BUFFER_POOL_INGRESS,
    SAI_BUFFER_POOL_EGRESS
};

DEFINE_ENUM_VALUES(sai_buffer_threshold_mode_t)
{
    SAI_BUFFER_THRESHOLD_MODE_STATIC,
    SAI_BUFFER_THRESHOLD_MODE_DYNAMIC
};

const sai_attr_metadata_t sai_buffer_pool_attr_metadata[] = {

    {
        .objecttype             = SAI_OBJECT_TYPE_BUFFER_POOL,
        .attrid                 = SAI_BUFFER_POOL_ATTR_SHARED_SIZE,
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
        .objecttype             = SAI_OBJECT_TYPE_BUFFER_POOL,
        .attrid                 = SAI_BUFFER_POOL_ATTR_TYPE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = StringifyEnum ( sai_buffer_pool_type_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_buffer_pool_type_t ),
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_BUFFER_POOL,
        .attrid                 = SAI_BUFFER_POOL_ATTR_SIZE,
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
        .objecttype             = SAI_OBJECT_TYPE_BUFFER_POOL,
        .attrid                 = SAI_BUFFER_POOL_ATTR_TH_MODE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .s32 = SAI_BUFFER_THRESHOLD_MODE_DYNAMIC },
        .enumtypestr            = StringifyEnum ( sai_buffer_threshold_mode_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_buffer_threshold_mode_t ),
        .conditions             = { },
    },
};

const size_t sai_buffer_pool_attr_metadata_count = sizeof(sai_buffer_pool_attr_metadata)/sizeof(sai_attr_metadata_t);

// METADATA for SAI_OBJECT_TYPE_BUFFER_PROFILE

const sai_attr_metadata_t sai_buffer_profile_attr_metadata[] = {

    {
        .objecttype             = SAI_OBJECT_TYPE_BUFFER_PROFILE,
        .attrid                 = SAI_BUFFER_PROFILE_ATTR_POOL_ID,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_BUFFER_POOL },
        .allownullobjectid      = true,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },

        // Pool id = SAI_NULL_OBJECT_ID can be used when profile is not associated with specific
        // pool, for example for global port buffer. Not applicable to priority group or queue buffer profile
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_BUFFER_PROFILE,
        .attrid                 = SAI_BUFFER_PROFILE_ATTR_BUFFER_SIZE,
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
        .objecttype             = SAI_OBJECT_TYPE_BUFFER_PROFILE,
        .attrid                 = SAI_BUFFER_PROFILE_ATTR_TH_MODE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_INHERIT, // TODO we need to specify from which inherit .oid?
        .defaultvalue           = { },
        .enumtypestr            = StringifyEnum ( sai_buffer_threshold_mode_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_buffer_threshold_mode_t ), // TODO 3rd value needed to use pool and set to default (PR in progress)
        .conditions             = { },

        // If set, this overrides SAI_BUFFER_POOL_ATTR_TH_MODE.
        // If not set, use SAI_BUFFER_POOL_ATTR_TH_MODE.
        // (default to value set in SAI_BUFFER_POOL_ATTR_TH_MODE.)
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_BUFFER_PROFILE,
        .attrid                 = SAI_BUFFER_PROFILE_ATTR_SHARED_DYNAMIC_TH,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT8,
        .flags                  = /*SAI_ATTR_FLAGS_MANDATORY_ON_CREATE |*/ SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { }, // ENUM_COND ( SAI_BUFFER_PROFILE_ATTR_TH_MODE, SAI_BUFFER_THRESHOLD_MODE_DYNAMIC ),

        // TODO condition here is complex, since if TH is inherited from pool then this is hard to determine
        // and it should be SAI_BUFFER_POOL_TH_MODE = SAI_BUFFER_THRESHOLD_MODE_DYNAMIC, so this attribute depends on other attribute
        // FIXME: SAI_ATTR_FLAGS_MANDATORY_ON_CREATE was temporary comment out until SAI has better support for it
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_BUFFER_PROFILE,
        .attrid                 = SAI_BUFFER_PROFILE_ATTR_SHARED_STATIC_TH,
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT32,
        .flags                  = /*SAI_ATTR_FLAGS_MANDATORY_ON_CREATE |*/ SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { }, //  ENUM_COND ( SAI_BUFFER_PROFILE_ATTR_TH_MODE, SAI_BUFFER_THRESHOLD_MODE_DYNAMIC ),

        // TODO condition here is complex, since if TH is inherited from pool then this is hard to determine
        // and it should be SAI_BUFFER_POOL_TH_MODE = SAI_BUFFER_THRESHOLD_MODE_DYNAMIC, so this attribute depends on other attribute
        // FIXME: SAI_ATTR_FLAGS_MANDATORY_ON_CREATE was temporary comment out until SAI has better support for it    
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_BUFFER_PROFILE,
        .attrid                 = SAI_BUFFER_PROFILE_ATTR_XOFF_TH,
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .u32 = 0 },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },

        // TODO valid only for ingress priority group
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_BUFFER_PROFILE,
        .attrid                 = SAI_BUFFER_PROFILE_ATTR_XON_TH,
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .u32 = 0 },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },

        // TODO valid only for ingress priority group
    },
};

const size_t sai_buffer_profile_attr_metadata_count = sizeof(sai_buffer_profile_attr_metadata)/sizeof(sai_attr_metadata_t);

