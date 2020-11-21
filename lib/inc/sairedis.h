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

typedef enum _sai_redis_notify_syncd_t
{
    SAI_REDIS_NOTIFY_SYNCD_INIT_VIEW,

    SAI_REDIS_NOTIFY_SYNCD_APPLY_VIEW,

    SAI_REDIS_NOTIFY_SYNCD_INSPECT_ASIC

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
     * NOTE: This attribute is depreacated by
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

} sai_redis_switch_attr_t;
