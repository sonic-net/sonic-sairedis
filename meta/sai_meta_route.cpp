#include "sai_meta.h"

// METADATA for SAI_OBJECT_TYPE_ROUTE

const sai_attr_metadata_t sai_route_attr_metadata[] = {

    {
        .objecttype             = SAI_OBJECT_TYPE_ROUTE,
        .attrid                 = SAI_ROUTE_ATTR_PACKET_ACTION,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .s32 = SAI_PACKET_ACTION_FORWARD },
        .enumtypestr            = StringifyEnum ( sai_packet_action_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_packet_action_t ),
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ROUTE,
        .attrid                 = SAI_ROUTE_ATTR_TRAP_PRIORITY,
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT8,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .u8 = 0 },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ROUTE,
        .attrid                 = SAI_ROUTE_ATTR_NEXT_HOP_ID,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_NEXT_HOP, SAI_OBJECT_TYPE_NEXT_HOP_GROUP, SAI_OBJECT_TYPE_ROUTER_INTERFACE, SAI_OBJECT_TYPE_PORT },
        .allownullobjectid      = true,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },

        // TODO may require some extra logic for CPU port
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ROUTE,
        .attrid                 = SAI_ROUTE_ATTR_META_DATA,
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_ATTR_RANGE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },

        // TODO range in SAI_SWITCH_ATTR_ROUTE_USER_META_DATA_RANGE
    },
};

const size_t sai_route_attr_metadata_count = sizeof(sai_route_attr_metadata)/sizeof(sai_attr_metadata_t);
