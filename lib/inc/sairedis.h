#ifndef __SAIREDIS__
#define __SAIREDIS__

extern "C" {
#include "sai.h"
}

#define SYNCD_INIT_VIEW  "INIT_VIEW"
#define SYNCD_APPLY_VIEW "APPLY_VIEW"
#define SYNCD_INSPECT_ASIC "SYNCD_INSPECT_ASIC"
#define ASIC_STATE_TABLE "ASIC_STATE"
#define TEMP_PREFIX      "TEMP_"

// Messages for processing queries from libsairedis to syncd
const std::string attrEnumValuesCapabilityQuery("attr_enum_values_capability_query");
const std::string attrEnumValuesCapabilityResponse("attr_enum_values_capability_response");
const std::string objectTypeGetAvailabilityQuery("object_type_get_availability_query");
const std::string objectTypeGetAvailabilityResponse("object_type_get_availability_response");

/**
 * @brief Default synchronous operation response timeout in milliseconds.
 */
#define SAI_REDIS_DEFAULT_SYNC_OPERATION_RESPONSE_TIMEOUT (60*1000)

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
     * @brief Recording log filename.
     *
     * Default valus is sairedis.rec
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

} sai_redis_switch_attr_t;

/*
 * Those bulk APIs are provided as static functions since current SAI 0.9.4
 * doesn't support bulk API.  Also note that SDK don't support bulk route API
 * yet, so those APIs are introduced here to reduce number of calls to redis,
 * in syncd bulk API will be splitted to separate call's until proper SDK
 * support will be added.
 *
 * Currently bulk operation type is skipped, and all operation must end with
 * success.
 */

#ifndef SAI_STATUS_NOT_EXECUTED
#define SAI_STATUS_NOT_EXECUTED                     SAI_STATUS_CODE(0x00000017L)
#endif

#ifndef SAI_COMMON_API_BULK_SET
#define SAI_COMMON_API_BULK_CREATE 4
#define SAI_COMMON_API_BULK_REMOVE 5
#define SAI_COMMON_API_BULK_SET    6
#define SAI_COMMON_API_BULK_GET    7
#endif

/**
 * @brief Bulk create route entry
 *
 * @param[in] object_count Number of objects to create
 * @param[in] route_entry List of object to create
 * @param[in] attr_count List of attr_count. Caller passes the number
 *    of attribute for each object to create.
 * @param[in] attr_list List of attributes for every object.
 * @param[in] mode Bulk operation error handling mode
 * @param[out] object_statuses List of status for every object. Caller needs to
 * allocate the buffer
 *
 * @return #SAI_STATUS_SUCCESS on success when all objects are created or
 * #SAI_STATUS_FAILURE when any of the objects fails to create. When there is
 * failure, Caller is expected to go through the list of returned statuses to
 * find out which fails and which succeeds.
 */
sai_status_t sai_bulk_create_route_entry(
        _In_ uint32_t object_count,
        _In_ const sai_route_entry_t *route_entry,
        _In_ const uint32_t *attr_count,
        _In_ const sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses);

/**
 * @brief Bulk remove route entry
 *
 * @param[in] object_count Number of objects to remove
 * @param[in] route_entry List of objects to remove
 * @param[in] mode Bulk operation error handling mode
 * @param[out] object_statuses List of status for every object. Caller needs to
 * allocate the buffer
 *
 * @return #SAI_STATUS_SUCCESS on success when all objects are removed or
 * #SAI_STATUS_FAILURE when any of the objects fails to remove. When there is
 * failure, Caller is expected to go through the list of returned statuses to
 * find out which fails and which succeeds.
 */
sai_status_t sai_bulk_remove_route_entry(
        _In_ uint32_t object_count,
        _In_ const sai_route_entry_t *route_entry,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses);

/**
 * @brief Bulk set attribute on route entry
 *
 * @param[in] object_count Number of objects to set attribute
 * @param[in] route_entry List of objects to set attribute
 * @param[in] attr_list List of attributes to set on objects, one attribute per object
 * @param[in] mode Bulk operation error handling mode
 * @param[out] object_statuses List of status for every object. Caller needs to
 * allocate the buffer
 *
 * @return #SAI_STATUS_SUCCESS on success when all objects are removed or
 * #SAI_STATUS_FAILURE when any of the objects fails to remove. When there is
 * failure, Caller is expected to go through the list of returned statuses to
 * find out which fails and which succeeds.
 */
sai_status_t sai_bulk_set_route_entry_attribute(
        _In_ uint32_t object_count,
        _In_ const sai_route_entry_t *route_entry,
        _In_ const sai_attribute_t *attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses);

/**
 * @brief Bulk get attribute on route entry
 *
 * @param[in] object_count Number of objects to set attribute
 * @param[in] route_entry List of objects to set attribute
 * @param[in] attr_count List of attr_count. Caller passes the number
 *    of attribute for each object to get
 * @param[inout] attr_list List of attributes to set on objects, one attribute per object
 * @param[in] mode Bulk operation error handling mode
 * @param[out] object_statuses List of status for every object. Caller needs to
 * allocate the buffer
 *
 * @return #SAI_STATUS_SUCCESS on success when all objects are removed or
 * #SAI_STATUS_FAILURE when any of the objects fails to remove. When there is
 * failure, Caller is expected to go through the list of returned statuses to
 * find out which fails and which succeeds.
 */
sai_status_t sai_bulk_get_route_entry_attribute(
        _In_ uint32_t object_count,
        _In_ const sai_route_entry_t *route_entry,
        _In_ const uint32_t *attr_count,
        _Inout_ sai_attribute_t **attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses);

sai_status_t sai_bulk_create_next_hop_group_members(
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t object_count,
        _In_ const uint32_t *attr_count,
        _In_ const sai_attribute_t **attrs,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_object_id_t *object_id,
        _Out_ sai_status_t *object_statuses);

sai_status_t sai_bulk_remove_next_hop_group_members(
        _In_ uint32_t object_count,
        _In_ const sai_object_id_t *object_id,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses);

sai_status_t sai_bulk_create_fdb_entry(
        _In_ uint32_t object_count,
        _In_ const sai_fdb_entry_t *fdb_entry,
        _In_ const uint32_t *attr_count,
        _In_ const sai_attribute_t *const *attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses);

sai_status_t sai_bulk_remove_fdb_entry(
        _In_ uint32_t object_count,
        _In_ const sai_fdb_entry_t *fdb_entry,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses);
#endif // __SAIREDIS__
