#pragma once

/*
 * This header will contain definitions used by libsairedis and syncd as well
 * as libswsscommon (for producer/consumer) and lua scripts.
 */

#define SYNCD_INIT_VIEW     "INIT_VIEW"
#define SYNCD_APPLY_VIEW    "APPLY_VIEW"
#define SYNCD_INSPECT_ASIC  "SYNCD_INSPECT_ASIC"
#define ASIC_STATE_TABLE    "ASIC_STATE"
#define TEMP_PREFIX         "TEMP_"

/*
 * Asic state table commands. Those names are special and they will be used
 * inside swsscommon library LUA scripts to perform operations on redis
 * database.
 */

#define REDIS_ASIC_STATE_COMMAND_CREATE "create"
#define REDIS_ASIC_STATE_COMMAND_REMOVE "remove"
#define REDIS_ASIC_STATE_COMMAND_SET    "set"
#define REDIS_ASIC_STATE_COMMAND_GET    "get"

#define REDIS_ASIC_STATE_COMMAND_BULK_CREATE "bulkcreate"
#define REDIS_ASIC_STATE_COMMAND_BULK_REMOVE "bulkremove"
#define REDIS_ASIC_STATE_COMMAND_BULK_SET    "bulkset"
#define REDIS_ASIC_STATE_COMMAND_BULK_GET    "bulkget"

#define REDIS_ASIC_STATE_COMMAND_NOTIFY      "notify"

#define REDIS_ASIC_STATE_COMMAND_GET_STATS          "get_stats"
#define REDIS_ASIC_STATE_COMMAND_CLEAR_STATS        "clear_stats"

#define REDIS_ASIC_STATE_COMMAND_GETRESPONSE        "getresponse"

#define REDIS_ASIC_STATE_COMMAND_FLUSH              "flush"
#define REDIS_ASIC_STATE_COMMAND_FLUSHRESPONSE      "flushresponse"

#define REDIS_ASIC_STATE_COMMAND_ATTR_CAPABILITY_QUERY      "attribute_capability_query"
#define REDIS_ASIC_STATE_COMMAND_ATTR_CAPABILITY_RESPONSE   "attribute_capability_response"

#define REDIS_ASIC_STATE_COMMAND_ATTR_ENUM_VALUES_CAPABILITY_QUERY      "attr_enum_values_capability_query"
#define REDIS_ASIC_STATE_COMMAND_ATTR_ENUM_VALUES_CAPABILITY_RESPONSE   "attr_enum_values_capability_response"

#define REDIS_ASIC_STATE_COMMAND_OBJECT_TYPE_GET_AVAILABILITY_QUERY     "object_type_get_availability_query"
#define REDIS_ASIC_STATE_COMMAND_OBJECT_TYPE_GET_AVAILABILITY_RESPONSE  "object_type_get_availability_response"

// TODO move this to SAI meta repository for auto generate

#define SAI_SWITCH_NOTIFICATION_NAME_BFD_SESSION_STATE_CHANGE   "bfd_session_state_change"
#define SAI_SWITCH_NOTIFICATION_NAME_FDB_EVENT                  "fdb_event"
#define SAI_SWITCH_NOTIFICATION_NAME_PACKET_EVENT               "packet_event"
#define SAI_SWITCH_NOTIFICATION_NAME_PORT_STATE_CHANGE          "port_state_change"
#define SAI_SWITCH_NOTIFICATION_NAME_QUEUE_PFC_DEADLOCK         "queue_deadlock"
#define SAI_SWITCH_NOTIFICATION_NAME_SWITCH_SHUTDOWN_REQUEST    "switch_shutdown_request"
#define SAI_SWITCH_NOTIFICATION_NAME_SWITCH_STATE_CHANGE        "switch_state_change"
#define SAI_SWITCH_NOTIFICATION_NAME_TAM_EVENT                  "tam_event"

/**
 * @brief Redis virtual object id counter key name.
 *
 * This key will be used by sairedis and syncd in REDIS database to generate
 * new object indexes used when constructing new virtual object id (VID).
 *
 * This key must have atomic access since it can be used at any time by syncd
 * process or orchagent process.
 */
#define REDIS_KEY_VIDCOUNTER "VIDCOUNTER"

/**
 * @brief Table which will be used to forward notifications from syncd.
 */
#define REDIS_TABLE_NOTIFICATIONS   "NOTIFICATIONS"

/**
 * @brief Table which will be used to send API response from syncd.
 */
#define REDIS_TABLE_GETRESPONSE     "GETRESPONSE"

// REDIS default database defines

#define REDIS_DEFAULT_DATABASE_ASIC         "ASIC_DB"
#define REDIS_DEFAULT_DATABASE_STATE        "STATE_DB"
#define REDIS_DEFAULT_DATABASE_COUNTERS     "COUNTERS_DB"

// TODO to be removed (used only for plugin register)
#define REDIS_DEFAULT_DATABASE_FLEX_COUNTER "FLEX_COUNTER_DB"

