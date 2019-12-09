#ifndef __SAIREDIS__
#define __SAIREDIS__

extern "C" {
#include "sai.h"
}

typedef enum _sai_redis_notify_syncd_t
{
    SAI_REDIS_NOTIFY_SYNCD_INIT_VIEW,

    SAI_REDIS_NOTIFY_SYNCD_APPLY_VIEW,

    SAI_REDIS_NOTIFY_SYNCD_INSPECT_ASIC

} sai_redis_notify_syncd_t;

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
     * @type bool
     * @flags CREATE_AND_SET
     * @default false
     */
    SAI_REDIS_SWITCH_ATTR_SYNC_MODE,

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

} sai_redis_switch_attr_t;

#endif // __SAIREDIS__
