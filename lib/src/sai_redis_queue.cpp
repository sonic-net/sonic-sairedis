#include "sai_redis.h"

/**
 * Routine Description:
 *    @brief Create queue
 *
 * Arguments:
 *    @param[out] queue_id - queue id
 *    @param[in] attr_count - number of attributes
 *    @param[in] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 *
 */
sai_status_t redis_create_queue(
        _Out_ sai_object_id_t* queue_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_create_oid(
            SAI_OBJECT_TYPE_QUEUE,
            queue_id,
            attr_count,
            attr_list,
            &redis_generic_create);
}

/**
 * Routine Description:
 *    @brief Remove queue
 *
 * Arguments:
 *    @param[in] queue_id - queue id
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_remove_queue(
        _In_ sai_object_id_t queue_id)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_remove_oid(
            SAI_OBJECT_TYPE_QUEUE,
            queue_id,
            &redis_generic_remove);
}

/**
 * @brief Set attribute to Queue
 * @param[in] queue_id queue id to set the attribute
 * @param[in] attr attribute to set
 *
 * @return SAI_STATUS_SUCCESS on success
 *           Failure status code on error
 */
sai_status_t redis_set_queue_attribute(
        _In_ sai_object_id_t queue_id,
        _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_set_oid(
            SAI_OBJECT_TYPE_QUEUE,
            queue_id,
            attr,
            &redis_generic_set);
}

/**
 * @brief Get attribute to Queue
 * @param[in] queue_id queue id to set the attribute
 * @param[in] attr_count number of attributes
 * @param[inout] attr_list Array of attributes
 *
 * @return SAI_STATUS_SUCCESS on success
 *           Failure status code on error
 */
sai_status_t redis_get_queue_attribute(
        _In_ sai_object_id_t queue_id,
        _In_ uint32_t        attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_get_oid(
            SAI_OBJECT_TYPE_QUEUE,
            queue_id,
            attr_count,
            attr_list,
            &redis_generic_get);
}

/**
 * @brief   Get queue statistics counters.
 *
 * @param[in] queue_id Queue id
 * @param[in] counter_ids specifies the array of counter ids
 * @param[in] number_of_counters number of counters in the array
 * @param[out] counters array of resulting counter values.
 *
 * @return SAI_STATUS_SUCCESS on success
 *         Failure status code on error
 */
sai_status_t redis_get_queue_stats(
        _In_ sai_object_id_t queue_id,
        _In_ const sai_queue_stat_counter_t *counter_ids,
        _In_ uint32_t number_of_counters,
        _Out_ uint64_t* counters)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

/**
 * @brief   Clear queue statistics counters.
 *
 * @param[in] queue_id Queue id
 * @param[in] counter_ids specifies the array of counter ids
 * @param[in] number_of_counters number of counters in the array
 *
 * @return SAI_STATUS_SUCCESS on success
 *         Failure status code on error
 */
sai_status_t redis_clear_queue_stats(
        _In_ sai_object_id_t queue_id,
        _In_ const sai_queue_stat_counter_t *counter_ids,
        _In_ uint32_t number_of_counters)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

/**
 * @brief Qos methods table retrieved with sai_api_query()
 */
const sai_queue_api_t redis_queue_api = {
    redis_set_queue_attribute,
    redis_get_queue_attribute,
    redis_get_queue_stats,
    redis_clear_queue_stats,
};
