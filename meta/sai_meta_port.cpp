#include "sai_meta.h"

// METADATA for SAI_OBJECT_TYPE_PORT

DEFINE_ENUM_VALUES(sai_port_type_t)
{
    SAI_PORT_TYPE_LOGICAL,
    SAI_PORT_TYPE_CPU
};

DEFINE_ENUM_VALUES(sai_port_oper_status_t)
{
    SAI_PORT_OPER_STATUS_UNKNOWN,
    SAI_PORT_OPER_STATUS_UP,
    SAI_PORT_OPER_STATUS_DOWN,
    SAI_PORT_OPER_STATUS_TESTING,
    SAI_PORT_OPER_STATUS_NOT_PRESENT
};

DEFINE_ENUM_VALUES(sai_port_breakout_mode_type_t)
{
    SAI_PORT_BREAKOUT_MODE_1_LANE,
    SAI_PORT_BREAKOUT_MODE_2_LANE,
    SAI_PORT_BREAKOUT_MODE_4_LANE
};

DEFINE_ENUM_VALUES(sai_port_flow_control_mode_t)
{
    SAI_PORT_FLOW_CONTROL_DISABLE,
    SAI_PORT_FLOW_CONTROL_TX_ONLY,
    SAI_PORT_FLOW_CONTROL_RX_ONLY,
    SAI_PORT_FLOW_CONTROL_BOTH_ENABLE
};

DEFINE_ENUM_VALUES(sai_port_media_type_t)
{
    SAI_PORT_MEDIA_TYPE_NOT_PRESENT,
    SAI_PORT_MEDIA_TYPE_UNKNONWN,
    //SAI_PORT_MEDIA_TYPE_FIBER,
    //SAI_PORT_MEDIA_TYPE_COPPER
};

DEFINE_ENUM_VALUES(sai_port_internal_loopback_mode_t)
{
    SAI_PORT_INTERNAL_LOOPBACK_NONE,
    SAI_PORT_INTERNAL_LOOPBACK_PHY,
    SAI_PORT_INTERNAL_LOOPBACK_MAC
};

DEFINE_ENUM_VALUES(sai_port_fdb_learning_mode_t)
{
    SAI_PORT_LEARN_MODE_DROP,
    SAI_PORT_LEARN_MODE_DISABLE,
    SAI_PORT_LEARN_MODE_HW,
    SAI_PORT_LEARN_MODE_CPU_TRAP,
    SAI_PORT_LEARN_MODE_CPU_LOG
};

const sai_attr_metadata_t sai_port_attr_metadata[] = {

    // READ-ONLY

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_TYPE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_READ_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = StringifyEnum ( sai_port_type_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_port_type_t ),
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_OPER_STATUS,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_READ_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = StringifyEnum ( sai_port_oper_status_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_port_oper_status_t ),
        .conditions             = { },

        // dynamic
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_SUPPORTED_BREAKOUT_MODE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32_LIST,
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
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_CURRENT_BREAKOUT_MODE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_READ_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = StringifyEnum ( sai_port_breakout_mode_type_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_port_breakout_mode_type_t ),
        .conditions             = { },

        // dynamic
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_QOS_NUMBER_OF_QUEUES,
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT32,
        .flags                  = SAI_ATTR_FLAGS_READ_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },

        // dynamic
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_QOS_QUEUE_LIST,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_LIST,
        .flags                  = SAI_ATTR_FLAGS_READ_ONLY,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_QUEUE },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },

        // dynamic
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_QOS_NUMBER_OF_SCHEDULER_GROUPS,
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT32,
        .flags                  = SAI_ATTR_FLAGS_READ_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },

        // dynamic
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_QOS_SCHEDULER_GROUP_LIST,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_LIST,
        .flags                  = SAI_ATTR_FLAGS_READ_ONLY,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_SCHEDULER_GROUP },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },

        // dynamic
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_SUPPORTED_SPEED,
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT32_LIST,
        .flags                  = SAI_ATTR_FLAGS_READ_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },
/*
    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_SUPPORTED_HALF_DUPLEX_SPEED,
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT32_LIST,
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
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_SUPPORTED_AUTO_NEG_MODE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_BOOL,
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
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_SUPPORTED_FLOW_CONTROL,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_READ_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = StringifyEnum ( sai_port_flow_control_mode_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_port_flow_control_mode_t ),
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_SUPPORTED_ASYMMETRIC_PAUSE_MODE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_BOOL,
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
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_SUPPORTED_MEDIA_TYPE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_READ_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = StringifyEnum ( sai_port_media_type_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_port_media_type_t ),
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_REMOTE_SUPPORTED_SPEED,
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT32_LIST,
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
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_REMOTE_SUPPORTED_HALF_DUPLEX_SPEED,
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT32_LIST,
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
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_REMOTE_SUPPORTED_AUTO_NEG_MODE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_BOOL,
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
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_REMOTE_SUPPORTED_FLOW_CONTROL,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_READ_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = StringifyEnum ( sai_port_flow_control_mode_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_port_flow_control_mode_t ),
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_REMOTE_SUPPORTED_ASYMMETRIC_PAUSE_MODE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_BOOL,
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
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_REMOTE_SUPPORTED_MEDIA_TYPE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_READ_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = StringifyEnum ( sai_port_media_type_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_port_media_type_t ),
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_REMOTE_ADVERTISED_SPEED,
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT32_LIST,
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
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_REMOTE_ADVERTISED_HALF_DUPLEX_SPEED,
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT32_LIST,
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
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_REMOTE_ADVERTISED_AUTO_NEG_MODE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_BOOL,
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
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_REMOTE_ADVERTISED_FLOW_CONTROL,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_READ_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = StringifyEnum ( sai_port_flow_control_mode_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_port_flow_control_mode_t ),
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_REMOTE_ADVERTISED_ASYMMETRIC_PAUSE_MODE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_BOOL,
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
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_REMOTE_ADVERTISED_MEDIA_TYPE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_READ_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = StringifyEnum ( sai_port_media_type_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_port_media_type_t ),
        .conditions             = { },
    },
*/

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_NUMBER_OF_PRIORITY_GROUPS,
        .serializationtype      = SAI_SERIALIZATION_TYPE_BOOL,
        .flags                  = SAI_ATTR_FLAGS_READ_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },

        // dynamic
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_PRIORITY_GROUP_LIST,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_LIST,
        .flags                  = SAI_ATTR_FLAGS_READ_ONLY,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_PRIORITY_GROUP },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },

        // dynamic
    },

    // READ-WRITE

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_HW_LANE_LIST,
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT32_LIST,
        .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY | SAI_ATTR_FLAGS_KEY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_SPEED,
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
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_FULL_DUPLEX_MODE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .booldata = true },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_AUTO_NEG_MODE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_BOOL,
        .flags                  = SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .booldata = false },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_ADMIN_STATE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_BOOL,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .booldata = false },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_MEDIA_TYPE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .s32 = SAI_PORT_MEDIA_TYPE_NOT_PRESENT },
        .enumtypestr            = StringifyEnum ( sai_port_media_type_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_port_media_type_t ),
        .conditions             = { },
    },

    // TODO what is default value ?

    // {
    //     .objecttype             = SAI_OBJECT_TYPE_PORT,
    //     .attrid                 = SAI_PORT_ATTR_ADVERTISED_SPEED,
    //     .serializationtype      = SAI_SERIALIZATION_TYPE_UINT32_LIST,
    //     .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
    //     .allowedobjecttypes     = { },
    //     .allownullobjectid      = false,
    //     .defaultvalue           = { }, // TODO default
    //     .enumtypestr            = NULL,
    //     .enumallowedvalues      = { },
    //     .conditions             = { },
    // },

    // {
    //     .objecttype             = SAI_OBJECT_TYPE_PORT,
    //     .attrid                 = SAI_PORT_ATTR_ADVERTISED_HALF_DUPLEX_SPEED,
    //     .serializationtype      = SAI_SERIALIZATION_TYPE_UINT32_LIST,
    //     .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
    //     .allowedobjecttypes     = { },
    //     .allownullobjectid      = false,
    //     .defaultvalue           = { }, // TODO default
    //     .enumtypestr            = NULL,
    //     .enumallowedvalues      = { },
    //     .conditions             = { },
    // },

    // {
    //     .objecttype             = SAI_OBJECT_TYPE_PORT,
    //     .attrid                 = SAI_PORT_ATTR_ADVERTISED_AUTO_NEG_MODE,
    //     .serializationtype      = SAI_SERIALIZATION_TYPE_BOOL,
    //     .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
    //     .allowedobjecttypes     = { },
    //     .allownullobjectid      = false,
    //     .defaultvalue           = { }, // TODO default
    //     .enumtypestr            = NULL,
    //     .enumallowedvalues      = { },
    //     .conditions             = { },
    // },

    // {
    //     .objecttype             = SAI_OBJECT_TYPE_PORT,
    //     .attrid                 = SAI_PORT_ATTR_ADVERTISED_FLOW_CONTROL,
    //     .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
    //     .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
    //     .allowedobjecttypes     = { },
    //     .allownullobjectid      = false,
    //     .defaultvalue           = { }, // TODO default
    //     .enumtypestr            = StringifyEnum ( sai_port_flow_control_mode_t ),
    //     .enumallowedvalues      = ENUM_VALUES ( sai_port_flow_control_mode_t ),
    //     .conditions             = { },
    // },

    // {
    //     .objecttype             = SAI_OBJECT_TYPE_PORT,
    //     .attrid                 = SAI_PORT_ATTR_ADVERTISED_ASYMMETRIC_PAUSE_MODE,
    //     .serializationtype      = SAI_SERIALIZATION_TYPE_BOOL,
    //     .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
    //     .allowedobjecttypes     = { },
    //     .allownullobjectid      = false,
    //     .defaultvalue           = { }, // TODO default
    //     .enumtypestr            = NULL,
    //     .enumallowedvalues      = { },
    //     .conditions             = { },
    // },

    // {
    //     .objecttype             = SAI_OBJECT_TYPE_PORT,
    //     .attrid                 = SAI_PORT_ATTR_ADVERTISED_MEDIA_TYPE,
    //     .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
    //     .flags                  = SAI_ATTR_FLAGS_MANDATORY_ON_CREATE | SAI_ATTR_FLAGS_CREATE_ONLY,
    //     .allowedobjecttypes     = { },
    //     .allownullobjectid      = false,
    //     .defaultvalue           = { },
    //     .enumtypestr            = StringifyEnum ( sai_port_media_type_t ),
    //     .enumallowedvalues      = ENUM_VALUES ( sai_port_media_type_t ),
    //     .conditions             = { },
    // },

    // Continue

    // TODO this is inconsistent, since we have vlan member, so why this param is read/write?
    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_PORT_VLAN_ID,
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT16,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .u16 = DEFAULT_VLAN_NUMBER },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },

        // TODO extra logic needed to check if vlan exist
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_DEFAULT_VLAN_PRIORITY,
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
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_INGRESS_FILTERING,
        .serializationtype      = SAI_SERIALIZATION_TYPE_BOOL,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .booldata = false },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_DROP_UNTAGGED,
        .serializationtype      = SAI_SERIALIZATION_TYPE_BOOL,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .booldata = false },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_DROP_TAGGED,
        .serializationtype      = SAI_SERIALIZATION_TYPE_BOOL,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .booldata = false },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_INTERNAL_LOOPBACK,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .s32 = SAI_PORT_INTERNAL_LOOPBACK_NONE },
        .enumtypestr            = StringifyEnum ( sai_port_internal_loopback_mode_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_port_internal_loopback_mode_t ),
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_FDB_LEARNING,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .s32 = SAI_PORT_LEARN_MODE_HW },
        .enumtypestr            = StringifyEnum ( sai_port_fdb_learning_mode_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_port_fdb_learning_mode_t ),
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_UPDATE_DSCP,
        .serializationtype      = SAI_SERIALIZATION_TYPE_BOOL,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .booldata = false },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_MTU,
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .u32 = DEFAULT_LAYER2_FRAME_SIZE },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },

        // TODO extra check for range will be needed
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_FLOOD_STORM_CONTROL_POLICER_ID,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_POLICER },
        .allownullobjectid      = true,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .oid = SAI_NULL_OBJECT_ID }, // TODO switch may have default value assigned
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_BROADCAST_STORM_CONTROL_POLICER_ID,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_POLICER },
        .allownullobjectid      = true,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .oid = SAI_NULL_OBJECT_ID }, // TODO switch may have default value assigned
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_MULTICAST_STORM_CONTROL_POLICER_ID,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_POLICER },
        .allownullobjectid      = true,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .oid = SAI_NULL_OBJECT_ID }, // TODO switch may have default value assigned
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_GLOBAL_FLOW_CONTROL,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .s32 = SAI_PORT_FLOW_CONTROL_DISABLE },
        .enumtypestr            = StringifyEnum ( sai_port_flow_control_mode_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_port_flow_control_mode_t ),
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_MAX_LEARNED_ADDRESSES,
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .u32 = 0 },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_FDB_LEARNING_LIMIT_VIOLATION,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .s32 = SAI_PACKET_ACTION_DROP },
        .enumtypestr            = StringifyEnum ( sai_packet_action_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_packet_action_t ),
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_INGRESS_MIRROR_SESSION,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_LIST,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_MIRROR },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_EMPTY_LIST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_EGRESS_MIRROR_SESSION,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_LIST,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_MIRROR },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_EMPTY_LIST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_INGRESS_SAMPLEPACKET_ENABLE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_SAMPLEPACKET },
        .allownullobjectid      = true,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .oid = SAI_NULL_OBJECT_ID },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_EGRESS_SAMPLEPACKET_ENABLE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_SAMPLEPACKET },
        .allownullobjectid      = true,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .oid = SAI_NULL_OBJECT_ID },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_POLICER_ID,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_POLICER },
        .allownullobjectid      = true,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .oid = SAI_NULL_OBJECT_ID },    // TODO by default port may have policer assigned ?
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    // QOS

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_QOS_DEFAULT_TC,
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
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_QOS_DOT1P_TO_TC_MAP,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_QOS_MAPS },
        .allownullobjectid      = true,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .oid = SAI_NULL_OBJECT_ID },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_QOS_DOT1P_TO_COLOR_MAP,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_QOS_MAPS },
        .allownullobjectid      = true,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .oid = SAI_NULL_OBJECT_ID },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_QOS_DSCP_TO_TC_MAP,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_QOS_MAPS },
        .allownullobjectid      = true,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .oid = SAI_NULL_OBJECT_ID },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_QOS_DSCP_TO_COLOR_MAP,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_QOS_MAPS },
        .allownullobjectid      = true,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .oid = SAI_NULL_OBJECT_ID },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_QOS_TC_TO_QUEUE_MAP,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_QOS_MAPS },
        .allownullobjectid      = true,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .oid = SAI_NULL_OBJECT_ID },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_QOS_TC_AND_COLOR_TO_DOT1P_MAP,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_QOS_MAPS },
        .allownullobjectid      = true,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .oid = SAI_NULL_OBJECT_ID },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_QOS_TC_AND_COLOR_TO_DSCP_MAP,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_QOS_MAPS },
        .allownullobjectid      = true,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .oid = SAI_NULL_OBJECT_ID },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_QOS_TC_TO_PRIORITY_GROUP_MAP,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_QOS_MAPS },
        .allownullobjectid      = true,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .oid = SAI_NULL_OBJECT_ID },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_QOS_PFC_PRIORITY_TO_PRIORITY_GROUP_MAP,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_QOS_MAPS },
        .allownullobjectid      = true,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .oid = SAI_NULL_OBJECT_ID },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_QOS_PFC_PRIORITY_TO_QUEUE_MAP,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_QOS_MAPS },
        .allownullobjectid      = true,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .oid = SAI_NULL_OBJECT_ID },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_QOS_WRED_PROFILE_ID,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_WRED },
        .allownullobjectid      = true,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .oid = SAI_NULL_OBJECT_ID },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_QOS_SCHEDULER_PROFILE_ID,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_SCHEDULER },
        .allownullobjectid      = true,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .oid = SAI_NULL_OBJECT_ID }, // TODO by default port may have scheduler
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_QOS_INGRESS_BUFFER_PROFILE_LIST,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_LIST,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_BUFFER_PROFILE },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_EMPTY_LIST,
        .defaultvalue           = { }, // TODO by default port may have ingress buffer profile
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_QOS_EGRESS_BUFFER_PROFILE_LIST,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_LIST,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_BUFFER_PROFILE },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_EMPTY_LIST,
        .defaultvalue           = { }, // TODO by default port may have egress buffer profile
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_PRIORITY_FLOW_CONTROL,
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
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_META_DATA,
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .u32 = 0 },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    /*{
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_EGRESS_BLOCK_PORT_LIST,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_LIST,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_BUFFER_PROFILE },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_EMPTY_LIST,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_PORT,
        .attrid                 = SAI_PORT_ATTR_HW_PROFILE_ID,
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT64,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .u64 = 0 },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },*/
};

const size_t sai_port_attr_metadata_count = sizeof(sai_port_attr_metadata)/sizeof(sai_attr_metadata_t);
