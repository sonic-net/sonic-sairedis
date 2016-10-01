#include "sai_meta.h"

// METADATA for SAI_OBJECT_TYPE_SWITCH

DEFINE_ENUM_VALUES(sai_packet_action_t)
{
    SAI_PACKET_ACTION_DROP,
    SAI_PACKET_ACTION_FORWARD,
    SAI_PACKET_ACTION_COPY,
    SAI_PACKET_ACTION_COPY_CANCEL,
    SAI_PACKET_ACTION_TRAP,
    SAI_PACKET_ACTION_LOG,
    SAI_PACKET_ACTION_DENY,
    SAI_PACKET_ACTION_TRANSIT
};

DEFINE_ENUM_VALUES(sai_switch_oper_status_t)
{
    SAI_SWITCH_OPER_STATUS_UNKNOWN,
    SAI_SWITCH_OPER_STATUS_UP,
    SAI_SWITCH_OPER_STATUS_DOWN,
    SAI_SWITCH_OPER_STATUS_FAILED
};

DEFINE_ENUM_VALUES(sai_switch_restart_type_t)
{
    SAI_RESTART_TYPE_NONE,
    SAI_RESTART_TYPE_PLANNED,
    SAI_RESTART_TYPE_ANY
};

DEFINE_ENUM_VALUES(sai_switch_switching_mode_t)
{
    SAI_SWITCHING_MODE_CUT_THROUGH,
    SAI_SWITCHING_MODE_STORE_AND_FORWARD
};

DEFINE_ENUM_VALUES(sai_hash_algorithm_t)
{
    SAI_HASH_ALGORITHM_CRC,
    SAI_HASH_ALGORITHM_XOR,
    SAI_HASH_ALGORITHM_RANDOM
};

const sai_attr_metadata_t sai_switch_attr_metadata[] = {

    // READ-ONLY

    {
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_PORT_NUMBER,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_PORT_LIST,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_LIST,
        .flags                  = SAI_ATTR_FLAGS_READ_ONLY,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_PORT },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },

        // dynamic
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_PORT_MAX_MTU,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_CPU_PORT,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_READ_ONLY,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_PORT },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_MAX_VIRTUAL_ROUTERS,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_FDB_TABLE_SIZE,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_L3_NEIGHBOR_TABLE_SIZE,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_L3_ROUTE_TABLE_SIZE,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_LAG_MEMBERS,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_NUMBER_OF_LAGS,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_ECMP_MEMBERS,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_NUMBER_OF_ECMP_GROUPS,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_NUMBER_OF_UNICAST_QUEUES,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_NUMBER_OF_MULTICAST_QUEUES,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_NUMBER_OF_QUEUES,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_NUMBER_OF_CPU_QUEUES,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_ON_LINK_ROUTE_SUPPORTED,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_OPER_STATUS,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_READ_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = StringifyEnum ( sai_switch_oper_status_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_switch_oper_status_t ),
        .conditions             = { },

        // dynamic
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_MAX_TEMP,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_ACL_TABLE_MINIMUM_PRIORITY,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_ACL_TABLE_MAXIMUM_PRIORITY,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_ACL_ENTRY_MINIMUM_PRIORITY,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_ACL_ENTRY_MAXIMUM_PRIORITY,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_FDB_DST_USER_META_DATA_RANGE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT32_RANGE,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_ROUTE_DST_USER_META_DATA_RANGE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT32_RANGE,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_NEIGHBOR_DST_USER_META_DATA_RANGE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT32_RANGE,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_PORT_USER_META_DATA_RANGE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT32_RANGE,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_VLAN_USER_META_DATA_RANGE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT32_RANGE,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_ACL_USER_META_DATA_RANGE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT32_RANGE,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_ACL_USER_TRAP_ID_RANGE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT32_RANGE,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_DEFAULT_STP_INST_ID,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_READ_ONLY,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_STP_INSTANCE },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_DEFAULT_VIRTUAL_ROUTER_ID,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_READ_ONLY,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_VIRTUAL_ROUTER },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_QOS_MAX_NUMBER_OF_TRAFFIC_CLASSES,
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT8,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_QOS_MAX_NUMBER_OF_SCHEDULER_GROUP_HIERARCHY_LEVELS,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_QOS_MAX_NUMBER_OF_SCHEDULER_GROUPS_PER_HIERARCHY_LEVEL,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_QOS_MAX_NUMBER_OF_CHILDS_PER_SCHEDULER_GROUP,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_TOTAL_BUFFER_SIZE,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_INGRESS_BUFFER_POOL_NUM,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_EGRESS_BUFFER_POOL_NUM,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_DEFAULT_TRAP_GROUP,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_READ_ONLY,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_TRAP_GROUP },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },

        /*
         * (default value after switch initialization
         *    SAI_HOSTIF_TRAP_GROUP_ATTR_ADMIN_STATE = true
         *    SAI_HOSTIF_TRAP_GROUP_ATTR_PRIO = SAI_SWITCH_ATTR_ACL_TABLE_MINIMUM_PRIORITY,
         *    SAI_HOSTIF_TRAP_GROUP_ATTR_QUEUE = 0,
         *    SAI_HOSTIF_TRAP_GROUP_ATTR_POLICER = SAI_NULL_OBJECT_ID)
         * The group handle is read only, while the group attributes, such as queue and policer,
         * may be modified
         */
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_ECMP_HASH,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_READ_ONLY,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_HASH },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },

        /*
         * (default value after switch initialization
         *   SAI_HASH_NATIVE_FIELD_LIST = [SAI_NATIVE_HASH_FIELD_SRC_MAC,
         *   SAI_NATIVE_HASH_FIELD_DST_MAC, SAI_NATIVE_HASH_FIELD_IN_PORT,
         *   SAI_NATIVE_HASH_FIELD_ETHERTYPE]
         *   SAI_HASH_UDF_GROUP_LIST empty list)
         * The object id is read only, while the object attributes can be modified
         */
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_LAG_HASH,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_READ_ONLY,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_HASH },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },

        /*
         * (default value after switch initialization
         *   SAI_HASH_NATIVE_FIELD_LIST = [SAI_NATIVE_HASH_FIELD_SRC_MAC,
         *   SAI_NATIVE_HASH_FIELD_DST_MAC, SAI_NATIVE_HASH_FIELD_IN_PORT,
         *   SAI_NATIVE_HASH_FIELD_ETHERTYPE]
         *   SAI_HASH_UDF_GROUP_LIST empty list)
         * The object id is read only, while the object attributes can be modified
         */
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_RESTART_TYPE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_READ_ONLY,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
        .defaultvalue           = { },
        .enumtypestr            = StringifyEnum ( sai_switch_restart_type_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_switch_restart_type_t ),
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_MIN_PLANNED_RESTART_INTERVAL,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_NV_STORAGE_SIZE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT64,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_MAX_ACL_ACTION_COUNT,
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

//    {
//        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
//        .attrid                 = SAI_SWITCH_ATTR_ACL_CAPABILITY,
//        .serializationtype      = SAI_SERIALIZATION_TYPE_ACL_CAPABILITY, // not supported yet
//        .flags                  = SAI_ATTR_FLAGS_READ_ONLY,
//        .allowedobjecttypes     = { },
//        .allownullobjectid      = false,
//        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_NONE,
//        .defaultvalue           = { },
//        .enumtypestr            = NULL,
//        .enumallowedvalues      = { },
//        .conditions             = { },
//    },

    // READ-WRITE

    {
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_SWITCHING_MODE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .s32 = SAI_SWITCHING_MODE_STORE_AND_FORWARD },
        .enumtypestr            = StringifyEnum ( sai_switch_switching_mode_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_switch_switching_mode_t ),
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_BCAST_CPU_FLOOD_ENABLE,
        .serializationtype      = SAI_SERIALIZATION_TYPE_BOOL,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .booldata = false },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },

        // TODO what is default value? switch dependent?
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_MCAST_CPU_FLOOD_ENABLE,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_SRC_MAC_ADDRESS,
        .serializationtype      = SAI_SERIALIZATION_TYPE_MAC,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_VENDOR_SPECIFIC,
        .defaultvalue           = { },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },

        // NOTE: Default value is switch dependent
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_MAX_LEARNED_ADDRESSES,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_FDB_AGING_TIME,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_FDB_UNICAST_MISS_ACTION,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_FDB_BROADCAST_MISS_ACTION,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_FDB_MULTICAST_MISS_ACTION,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_ECMP_DEFAULT_HASH_ALGORITHM,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .s32 = SAI_HASH_ALGORITHM_CRC },
        .enumtypestr            = StringifyEnum ( sai_hash_algorithm_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_hash_algorithm_t ),
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_ECMP_DEFAULT_HASH_SEED,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_ECMP_DEFAULT_SYMMETRIC_HASH,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_ECMP_HASH_IPV4,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_HASH },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .oid = SAI_NULL_OBJECT_ID },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },

        // TODO there may exist internal hash (vendor specific / switch internal)
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_ECMP_HASH_IPV4_IN_IPV4,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_HASH },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .oid = SAI_NULL_OBJECT_ID },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },

        // TODO there may exist internal hash
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_ECMP_HASH_IPV6,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_HASH },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .oid = SAI_NULL_OBJECT_ID },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },

        // TODO there may exist internal hash
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_LAG_DEFAULT_HASH_ALGORITHM,
        .serializationtype      = SAI_SERIALIZATION_TYPE_INT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .s32 = SAI_HASH_ALGORITHM_CRC },
        .enumtypestr            = StringifyEnum ( sai_hash_algorithm_t ),
        .enumallowedvalues      = ENUM_VALUES ( sai_hash_algorithm_t ),
        .conditions             = { },
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_LAG_DEFAULT_HASH_SEED,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_LAG_DEFAULT_SYMMETRIC_HASH,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_LAG_HASH_IPV4,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_HASH },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .oid = SAI_NULL_OBJECT_ID },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },

        .conditions             = { },

        // TODO there may exist internal hash
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_LAG_HASH_IPV4_IN_IPV4,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_HASH },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .oid = SAI_NULL_OBJECT_ID },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },

        // TODO there may exist internal hash
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_LAG_HASH_IPV6,
        .serializationtype      = SAI_SERIALIZATION_TYPE_OBJECT_ID,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { SAI_OBJECT_TYPE_HASH },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .oid = SAI_NULL_OBJECT_ID },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },

        .conditions             = { },

        // TODO there may exist internal hash
    },

    {
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_COUNTER_REFRESH_INTERVAL,
        .serializationtype      = SAI_SERIALIZATION_TYPE_UINT32,
        .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
        .allowedobjecttypes     = { },
        .allownullobjectid      = false,
        .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
        .defaultvalue           = { .u32 = 1 },
        .enumtypestr            = NULL,
        .enumallowedvalues      = { },
        .conditions             = { },
    },

    // QOS attributes
    // extra validation may be needed against PORT and map on ports

    {
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_QOS_DEFAULT_TC,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_QOS_DOT1P_TO_TC_MAP,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_QOS_DOT1P_TO_COLOR_MAP,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_QOS_DSCP_TO_TC_MAP,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_QOS_DSCP_TO_COLOR_MAP,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_QOS_TC_TO_QUEUE_MAP,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_QOS_TC_AND_COLOR_TO_DOT1P_MAP,
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
        .objecttype             = SAI_OBJECT_TYPE_SWITCH,
        .attrid                 = SAI_SWITCH_ATTR_QOS_TC_AND_COLOR_TO_DSCP_MAP,
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

    //{
    //    .objecttype             = SAI_OBJECT_TYPE_SWITCH,
    //    .attrid                 = SAI_SWITCH_ATTR_SWITCH_SHELL_ENABLE,
    //    .serializationtype      = SAI_SERIALIZATION_TYPE_BOOL,
    //    .flags                  = SAI_ATTR_FLAGS_CREATE_AND_SET,
    //    .allowedobjecttypes     = { },
    //    .allownullobjectid      = true,
    //    .defaultvaluetype       = SAI_DEFAULT_VALUE_TYPE_CONST,
    //    .defaultvalue           = { .booldata = false },
    //    .enumtypestr            = NULL,
    //    .enumallowedvalues      = { },
    //    .conditions             = { },
    //},
};

const size_t sai_switch_attr_metadata_count = sizeof(sai_switch_attr_metadata)/sizeof(sai_attr_metadata_t);
