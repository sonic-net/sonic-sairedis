#pragma once

extern "C" {
#include "sai.h"
}

/**
 * @brief Redis key context config.
 *
 * Optional. Should point to a context_config.json which will contain how many
 * contexts (syncd) we have in the system globally and each context how many
 * switches it manages.
 */
#define SAI_REDIS_KEY_CONTEXT_CONFIG              "SAI_REDIS_CONTEXT_CONFIG"

/**
 * @brief Redis enable client.
 *
 * Optional. By default sairedis act as sairedis server, which means it
 * directly talks to syncd. If this key is set to "true", then this instance of
 * sairedis will act as a client, and it will connect and talk to sairedis
 * server instance only.  There can be multiple clients active. Default value
 * is "false".
 */
#define SAI_REDIS_KEY_ENABLE_CLIENT               "SAI_REDIS_ENABLE_CLIENT"

/**
 * @brief Redis client config.
 *
 * Optional. Should point to client_config.json file which contains
 * client/server channel configuration for client side.
 */
#define SAI_REDIS_KEY_CLIENT_CONFIG               "SAI_REDIS_CLIENT_CONFIG"

/**
 * @brief Redis server config.
 *
 * Optional. Should point to server_config.json file which contains
 * client/server channel configuration for server side.
 */
#define SAI_REDIS_KEY_SERVER_CONFIG               "SAI_REDIS_SERVER_CONFIG"

/**
 * @brief Default synchronous operation response timeout in milliseconds.
 */
#define SAI_REDIS_DEFAULT_SYNC_OPERATION_RESPONSE_TIMEOUT (80*1000)

typedef enum _sai_redis_notify_syncd_t
{
    SAI_REDIS_NOTIFY_SYNCD_INIT_VIEW,

    SAI_REDIS_NOTIFY_SYNCD_APPLY_VIEW,

    SAI_REDIS_NOTIFY_SYNCD_INSPECT_ASIC,

    SAI_REDIS_NOTIFY_SYNCD_INVOKE_DUMP
} sai_redis_notify_syncd_t;

typedef enum _sai_redis_communication_mode_t
{
    /**
     * @brief Asynchronous mode using Redis DB.
     */
    SAI_REDIS_COMMUNICATION_MODE_REDIS_ASYNC,

    /**
     * @brief Synchronous mode using Redis DB.
     */
    SAI_REDIS_COMMUNICATION_MODE_REDIS_SYNC,

    /**
     * @brief Synchronous mode using ZMQ library.
     *
     * When enabled syncd also needs to be running in zmq synchronous mode.
     * Command pipeline will be disabled when this flag will be set to true.
     *
     * This attribute is only introduced to help kick start using synchronous
     * mode with zmq library. This mode requires some additional configuration
     * like main channel string and notification channel string. When using
     * this attribute those channels are set to default values:
     * "ipc:///tmp/zmq_ep" and "ipc:///tmp/zmq_ntf_ep". To take control of
     * those values a context config json file must be provided via
     * SAI_REDIS_KEY_CONTEXT_CONFIG profile argument.
     */
    SAI_REDIS_COMMUNICATION_MODE_ZMQ_SYNC,

} sai_redis_communication_mode_t;

/**
 * @brief Use Redis communication channel to handle counters.
 *
 * Originally, there are out-of-order issue between the objects and their counters.
 * This is because the counters are handled on receiving flex counter database update.
 * However, the objects are handled on receiving update from the redis communication channel.
 *
 * To resolve the issue, we use the redis communication channel to handle the counter updates.
 *
 * The struct sai_redis_flex_counter_group_parameter_t represents the counter group operations.
 * The caller (usually orchagent) can change some or all the options of a counter group.
 * The counter_group_name represents the counter group name which must be a valid list.
 * For the rest fields, it means not changing it to pass an empty list.
 */
typedef struct _sai_redis_flex_counter_group_parameter_t
{
    /**
     * @brief The flex counter group name.
     *
     * It is the key of FLEX_COUNTER_TABLE and FLEX_COUNTER_GROUP_TABLE table.
     */
    sai_s8_list_t counter_group_name;

    /**
     * @brief The polling interval of the counter group
     *
     * It should be a number representing the polling interval in seconds.
     */
    sai_s8_list_t poll_interval;

    /**
     * @brief The operation of the counter group
     *
     * It should be either "enable" or "disable"
     */
    sai_s8_list_t operation;

    /**
     * @brief The counter fetching mode.
     *
     * It should be either "STATS_MODE_READ" or "STATS_MODE_READ_AND_CLEAR"
     */
    sai_s8_list_t stats_mode;

    /**
     * @brief The name of the filed that represents the Lua plugin
     */
    sai_s8_list_t plugin_name;

    /**
     * @brief The SHA code of the Lua plugin
     */
    sai_s8_list_t plugins;

} sai_redis_flex_counter_group_parameter_t;

typedef struct _sai_redis_flex_counter_parameter_t
{
    /**
     * @brief The key in the flex counter table
     *
     * It should be the serialized OID eg. "oid:0x15000000000001"
     */
    sai_s8_list_t counter_key;

    /**
     * @brief The list of counters' IDs that should be fetched.
     */
    sai_s8_list_t counter_ids;

    /**
     * @brief The name of the filed that represents the counters' IDs.
     */
    sai_s8_list_t counter_field_name;

    /**
     * @brief The counter fetch mode of the object.
     */
    sai_s8_list_t stats_mode;

} sai_redis_flex_counter_parameter_t;

typedef enum _sai_redis_switch_attr_t
{
    /**
     * @brief Will start or stop recording history file for player
     *
     * @type bool
     * @flags CREATE_AND_SET
     * @default true
     */
    SAI_REDIS_SWITCH_ATTR_RECORD = SAI_SWITCH_ATTR_CUSTOM_RANGE_START,

    /**
     * @brief Will notify syncd whether to init or apply view
     *
     * @type sai_redis_notify_syncd_t
     * @flags CREATE_AND_SET
     * @default SAI_REDIS_NOTIFY_SYNCD_APPLY_VIEW
     */
    SAI_REDIS_SWITCH_ATTR_NOTIFY_SYNCD,

    /**
     * @brief Use temporary view for all actions between
     * init and apply view. By default init and apply view will
     * not take effect. This is temporary solution until
     * comparison logic will be in place.
     *
     * @type bool
     * @flags CREATE_AND_SET
     * @default false
     */
    SAI_REDIS_SWITCH_ATTR_USE_TEMP_VIEW,

    /**
     * @brief Enable redis pipeline
     *
     * @type bool
     * @flags CREATE_AND_SET
     * @default false
     */
    SAI_REDIS_SWITCH_ATTR_USE_PIPELINE,

    /**
     * @brief Will flush redis pipeline
     *
     * @type bool
     * @flags CREATE_AND_SET
     * @default false
     */
    SAI_REDIS_SWITCH_ATTR_FLUSH,

    /**
     * @brief Recording output directory.
     *
     * By default is current directory. Also setting empty will force default
     * directory.
     *
     * It will have only impact on next created recording.
     *
     * @type sai_s8_list_t
     * @flags CREATE_AND_SET
     * @default empty
     */
    SAI_REDIS_SWITCH_ATTR_RECORDING_OUTPUT_DIR,

    /**
     * @brief Log rotate.
     *
     * This is action attribute. When set to true then at the next log line
     * write it will close recording file and open it again. This is desired
     * when doing log rotate, since sairedis holds handle to recording file for
     * performance reasons. We are assuming logrotate will move recording file
     * to ".n" suffix, and when we reopen file, we will actually create new
     * one.
     *
     * This attribute is only setting variable in memory, it's safe to call
     * this from signal handler.
     *
     * @type bool
     * @flags CREATE_AND_SET
     * @default false
     */
    SAI_REDIS_SWITCH_ATTR_PERFORM_LOG_ROTATE,

    /**
     * @brief Synchronous mode.
     *
     * Enable or disable synchronous mode. When enabled syncd also needs to be
     * running in synchronous mode. Command pipeline will be disabled when this
     * flag will be set to true.
     *
     * NOTE: This attribute is deprecated by
     * SAI_REDIS_SWITCH_ATTR_REDIS_COMMUNICATION_MODE.  When set to true it
     * will set SAI_REDIS_SWITCH_ATTR_REDIS_COMMUNICATION_MODE to
     * SAI_REDIS_COMMUNICATION_MODE_REDIS_SYNC.
     *
     * TODO: remove this attribute.
     *
     * @type bool
     * @flags CREATE_AND_SET
     * @default false
     */
    SAI_REDIS_SWITCH_ATTR_SYNC_MODE,

    /**
     * @brief Redis communication mode.
     *
     * @type sai_redis_communication_mode_t
     * @flags CREATE_AND_SET
     * @default SAI_REDIS_COMMUNICATION_MODE_REDIS_ASYNC
     */
    SAI_REDIS_SWITCH_ATTR_REDIS_COMMUNICATION_MODE,

    /**
     * @brief Record statistics counters API calls.
     *
     * Get statistic counters can be queried periodically and can produce a lot
     * of logs in sairedis recording file. There are no OIDs retrieved in those
     * APIs, so user can disable recording statistics calls by setting this
     * attribute to false.
     *
     * @type bool
     * @flags CREATE_AND_SET
     * @default true
     */
    SAI_REDIS_SWITCH_ATTR_RECORD_STATS,

    /**
     * @brief Global context.
     *
     * When creating switch, this attribute can be specified (and must be
     * passed as last attribute on the list), will determine which context to
     * talk to.  Context is a syncd instance. Also this value is encoded
     * internally into each object ID, so each API call will know internally to
     * which instance of syncd send API requests.
     *
     * @type uint32_t
     * @flags CREATE_ONLY
     * @default 0
     */
    SAI_REDIS_SWITCH_ATTR_CONTEXT,

    /**
     * @brief Recording log filename.
     *
     * Default value is sairedis.rec
     *
     * @type sai_s8_list_t
     * @flags CREATE_AND_SET
     * @default sairedis.rec
     */
    SAI_REDIS_SWITCH_ATTR_RECORDING_FILENAME,

    /**
     * @brief Synchronous operation response timeout in milliseconds.
     *
     * Used for every synchronous API call. In asynchronous mode used for GET
     * operation.
     *
     * @type sai_uint64_t
     * @flags CREATE_AND_SET
     * @default 60000
     */
    SAI_REDIS_SWITCH_ATTR_SYNC_OPERATION_RESPONSE_TIMEOUT,

    /**
     * @brief Flex counter group operations
     *
     * @type sai_redis_flex_counter_group_parameter_t
     * @flags CREATE_AND_SET
     * @default 0
     */
    SAI_REDIS_SWITCH_ATTR_FLEX_COUNTER_GROUP,

    /**
     * @brief Flex counter operations
     *
     * @type sai_redis_counter_parameter_t
     * @flags CREATE_AND_SET
     * @default 0
     */
    SAI_REDIS_SWITCH_ATTR_FLEX_COUNTER,

} sai_redis_switch_attr_t;

/**
 * @brief Link event damping algorithms.
 */
typedef enum _sai_redis_link_event_damping_algorithm_t
{
    /** Link event damping algorithm disabled. */
    SAI_REDIS_LINK_EVENT_DAMPING_ALGORITHM_DISABLED = 0,

    /** Additive increase exponential decrease based link event damping algorithm. */
    SAI_REDIS_LINK_EVENT_DAMPING_ALGORITHM_AIED = 1,

} sai_redis_link_event_damping_algorithm_t;

typedef struct _sai_redis_link_event_damping_algo_aied_config_t
{
    /** Max link event damping suppression time (in milliseconds). */
    sai_uint32_t max_suppress_time;

    /** Link event damping suppress threshold. */
    sai_uint32_t suppress_threshold;

    /** Link event damping reuse threshold. */
    sai_uint32_t reuse_threshold;

    /** Link event damping decay half time duration (in milliseconds). */
    sai_uint32_t decay_half_life;

    /** Link event flap penalty. */
    sai_uint32_t flap_penalty;

} sai_redis_link_event_damping_algo_aied_config_t;

typedef enum _sai_redis_port_attr_t
{
    /**
     * @brief Link event damping algorithm.
     *
     * @type sai_redis_link_event_damping_algorithm_t
     * @flags CREATE_AND_SET
     * @default SAI_REDIS_LINK_EVENT_DAMPING_ALGORITHM_DISABLED
     */
    SAI_REDIS_PORT_ATTR_LINK_EVENT_DAMPING_ALGORITHM = SAI_PORT_ATTR_CUSTOM_RANGE_START,

    /**
     * @brief Link event damping AIED configuration.
     *
     * @type sai_redis_link_event_damping_algo_aied_config_t
     * @flags CREATE_AND_SET
     * @validonly SAI_REDIS_PORT_ATTR_LINK_EVENT_DAMPING_ALGORITHM = SAI_REDIS_LINK_EVENT_DAMPING_ALGORITHM_AIED
     * @default internal
     */
    SAI_REDIS_PORT_ATTR_LINK_EVENT_DAMPING_ALGO_AIED_CONFIG,

} sai_redis_port_attr_t;
