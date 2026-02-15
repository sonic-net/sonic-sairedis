#pragma once

/*
 * This header will contain definitions used by libsairedis and syncd as well
 * as libswsscommon (for producer/consumer) and lua scripts.
 */

#define SYNCD_INIT_VIEW     "INIT_VIEW"
#define SYNCD_APPLY_VIEW    "APPLY_VIEW"
#define SYNCD_INSPECT_ASIC  "SYNCD_INSPECT_ASIC"
#define SYNCD_INVOKE_DUMP   "SYNCD_INVOKE_DUMP"

#define ASIC_STATE_TABLE    "ASIC_STATE"
#define TEMP_PREFIX         "TEMP_"

#define REDIS_COMMUNICATION_MODE_REDIS_ASYNC_STRING "redis_async"
#define REDIS_COMMUNICATION_MODE_REDIS_SYNC_STRING  "redis_sync"
#define REDIS_COMMUNICATION_MODE_ZMQ_SYNC_STRING    "zmq_sync"

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

#define REDIS_FLEX_COUNTER_COMMAND_START_POLL       "start_poll"
#define REDIS_FLEX_COUNTER_COMMAND_STOP_POLL        "stop_poll"
#define REDIS_FLEX_COUNTER_COMMAND_SET_GROUP        "set_counter_group"
#define REDIS_FLEX_COUNTER_COMMAND_DEL_GROUP        "del_counter_group"
#define REDIS_FLEX_COUNTER_COMMAND_RESPONSE         "counter_response"

#define REDIS_ASIC_STATE_COMMAND_STATS_CAPABILITY_QUERY      "stats_capability_query"
#define REDIS_ASIC_STATE_COMMAND_STATS_CAPABILITY_RESPONSE   "stats_capability_response"

#define REDIS_ASIC_STATE_COMMAND_STATS_ST_CAPABILITY_QUERY "stats_st_capability_query"
#define REDIS_ASIC_STATE_COMMAND_STATS_ST_CAPABILITY_RESPONSE "stats_st_capability_response"

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
 * @brief Table which will be used to forward notifications per DB scope
 *
 * In https://redis.io/docs/manual/pubsub/, it says:
 * "Pub/Sub has no relation to the key space. It was made to not interfere with
 * it on any level, including database numbers."
 */
#define REDIS_TABLE_NOTIFICATIONS_PER_DB(dbName) \
    ((dbName) == "ASIC_DB" ? \
     REDIS_TABLE_NOTIFICATIONS : \
     (dbName) + "_" + REDIS_TABLE_NOTIFICATIONS)

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

