#include "sai_meta.h"

// METADATA for SAI_OBJECT_TYPE_VIRTUAL_ROUTER

const sai_attr_metadata_t sai_router_attr_metadata[] = {

    {
        .objecttype             = SAI_OBJECT_TYPE_VIRTUAL_ROUTER,
        .attrid                 = SAI_VIRTUAL_ROUTER_ATTR_ADMIN_V4_STATE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_BOOL,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .booldata = true },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_VIRTUAL_ROUTER,
        .attrid                 = SAI_VIRTUAL_ROUTER_ATTR_ADMIN_V6_STATE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_BOOL,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .booldata = true },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_VIRTUAL_ROUTER,
        .attrid                 = SAI_VIRTUAL_ROUTER_ATTR_SRC_MAC_ADDRESS,
        .serializationtype      = SAI_SERIALIZATION_TYPE_MAC,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_ATTR_VALUE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },

        // TODO equal to the SAI_SWITCH_ATTR_SRC_MAC_ADDRESS by default
        // when not specified it will use default mac address which is swich default
        // which is writtable and can be changed
        // TODO then metadata sanity must check that this attrib exist, is readonly and
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_VIRTUAL_ROUTER,
        .attrid                 = SAI_VIRTUAL_ROUTER_ATTR_VIOLATION_TTL1_ACTION,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .s32 = SAI_PACKET_ACTION_TRAP },
        .enumtypestr            = StringifyEnum ( sai_packet_action_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_packet_action_t ),
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_VIRTUAL_ROUTER,
        .attrid                 = SAI_VIRTUAL_ROUTER_ATTR_VIOLATION_IP_OPTIONS,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .s32 = SAI_PACKET_ACTION_TRAP },
        .enumtypestr            = StringifyEnum ( sai_packet_action_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_packet_action_t ),
        .conditions             = { },
    },
};

const size_t sai_router_attr_metadata_count = sizeof(sai_router_attr_metadata)/sizeof(sai_attr_metadata_t);
