#include "sai_vs.h"

/**
 * @brief Set attribute to Queue
 * @param[in] queue_id queue id to set the attribute
 * @param[in] attr attribute to set
 *
 * @return  SAI_STATUS_SUCCESS on success
 *           Failure status code on error
 */
sai_status_t  vs_set_queue_attribute(
    _In_ sai_object_id_t queue_id,
    _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_set(
            SAI_OBJECT_TYPE_QUEUE,
            queue_id,
            attr);

    return status;
}

/**
 * @brief Get attribute to Queue
 * @param[in] queue_id queue id to set the attribute
 * @param[in] attr_count number of attributes
 * @param[inout] attr_list Array of attributes
 *
 * @return  SAI_STATUS_SUCCESS on success
 *           Failure status code on error
 */
sai_status_t  vs_get_queue_attribute(
    _In_ sai_object_id_t queue_id,
    _In_ uint32_t        attr_count,
    _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_get(
            SAI_OBJECT_TYPE_QUEUE,
            queue_id,
            attr_count,
            attr_list);

    return status;
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
sai_status_t  vs_get_queue_stats(
    _In_ sai_object_id_t queue_id,
    _In_ const sai_queue_stat_counter_t *counter_ids,
    _In_ uint32_t number_of_counters,
    _Out_ uint64_t* counters)
{
    SWSS_LOG_ENTER();

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
sai_status_t  vs_clear_queue_stats(
    _In_ sai_object_id_t queue_id,
    _In_ const sai_queue_stat_counter_t *counter_ids,
    _In_ uint32_t number_of_counters)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}


/**
 *  @brief Qos methods table retrieved with sai_api_query()
 */
const sai_queue_api_t vs_queue_api  = {
    vs_set_queue_attribute,
    vs_get_queue_attribute,
    vs_get_queue_stats,
    vs_clear_queue_stats,
};
