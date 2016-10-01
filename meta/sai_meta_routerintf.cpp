#include "sai_meta.h"

// METADATA for SAI_OBJECT_TYPE_ROUTER_INTERFACE

DEFINE_ENUM_VALUES(sai_router_interface_type_t)
{
    SAI_ROUTER_INTERFACE_TYPE_PORT,
    SAI_ROUTER_INTERFACE_TYPE_VLAN,
    SAI_ROUTER_INTERFACE_TYPE_LOOPBACK
};

const sai_attr_metadata_t sai_routerintf_attr_metadata[] = {

    {
        .objecttype             = SAI_OBJECT_TYPE_ROUTER_INTERFACE,
        .attrid                 = SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_VIRTUAL_ROUTER },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ROUTER_INTERFACE,
        .attrid                 = SAI_ROUTER_INTERFACE_ATTR_TYPE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = StringifyEnum ( sai_router_interface_type_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_router_interface_type_t ),
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ROUTER_INTERFACE,
        .attrid                 = SAI_ROUTER_INTERFACE_ATTR_PORT_ID,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_PORT, SAI_OBJECT_TYPE_LAG },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { COND_ENUM ( SAI_ROUTER_INTERFACE_ATTR_TYPE, SAI_ROUTER_INTERFACE_TYPE_PORT ) },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ROUTER_INTERFACE,
        .attrid                 = SAI_ROUTER_INTERFACE_ATTR_VLAN_ID,
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT16,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { COND_ENUM ( SAI_ROUTER_INTERFACE_ATTR_TYPE, SAI_ROUTER_INTERFACE_TYPE_VLAN ) },

        // TODO extra check needed if vlan existsm and increase reference then
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ROUTER_INTERFACE,
        .attrid                 = SAI_ROUTER_INTERFACE_ATTR_SRC_MAC_ADDRESS,
        .serializationtype      = SAI_SERIALIZATION_TYPE_MAC,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_ATTR_VALUE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { }

        // TODO equal to the SAI_SWITCH_ATTR_SRC_MAC_ADDRESS by default
        // we need a way to provide attr type and attr id, type must match

        // TODO extra check: not valid when SAI_ROUTER_INTERFACE_ATTR_TYPE == SAI_ROUTER_INTERFACE_TYPE_LOOPBACK)
        // or valid only when:
        // { COND_ENUM ( SAI_ROUTER_INTERFACE_ATTR_TYPE, SAI_ROUTER_INTERFACE_TYPE_VLAN ) },
        // { COND_ENUM ( SAI_ROUTER_INTERFACE_ATTR_TYPE, SAI_ROUTER_INTERFACE_TYPE_PORT ) },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ROUTER_INTERFACE,
        .attrid                 = SAI_ROUTER_INTERFACE_ATTR_ADMIN_V4_STATE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_BOOL,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .booldata = true },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { }
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ROUTER_INTERFACE,
        .attrid                 = SAI_ROUTER_INTERFACE_ATTR_ADMIN_V6_STATE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_BOOL,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .booldata = true },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { }
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ROUTER_INTERFACE,
        .attrid                 = SAI_ROUTER_INTERFACE_ATTR_MTU,
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .u32 = DEFAULT_LAYER2_FRAME_SIZE },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { }
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_ROUTER_INTERFACE,
        .attrid                 = SAI_ROUTER_INTERFACE_ATTR_NEIGHBOR_MISS_PACKET_ACTION,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .s32 = SAI_PACKET_ACTION_TRAP },
        .enumtypestr            = StringifyEnum ( sai_packet_action_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_packet_action_t ),
        .conditions             = { }
    },
};

const size_t sai_routerintf_attr_metadata_count = sizeof(sai_routerintf_attr_metadata)/sizeof(sai_attr_metadata_t);
