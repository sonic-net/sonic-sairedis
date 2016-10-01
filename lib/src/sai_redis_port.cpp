#include "sai_redis.h"

/**
 * Routine Description:
 *   @brief Set port attribute value.
 *
 * Arguments:
 *    @param[in] port_id - port id
 *    @param[in] attr - attribute
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_set_port_attribute(
        _In_ sai_object_id_t port_id,
        _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_set_oid(
            SAI_OBJECT_TYPE_PORT,
            port_id,
            attr,
            &redis_generic_set);
}

/**
 * Routine Description:
 *   @brief Get port attribute value.
 *
 * Arguments:
 *    @param[in] port_id - port id
 *    @param[in] attr_count - number of attributes
 *    @param[inout] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_get_port_attribute(
        _In_ sai_object_id_t port_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_get_oid(
            SAI_OBJECT_TYPE_PORT,
            port_id,
            attr_count,
            attr_list,
            &redis_generic_get);
}

/**
 * Routine Description:
 *   @brief Get port statistics counters.
 *
 * Arguments:
 *    @param[in] port_id - port id
 *    @param[in] counter_ids - specifies the array of counter ids
 *    @param[in] number_of_counters - number of counters in the array
 *    @param[out] counters - array of resulting counter values.
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_get_port_stats(
        _In_ sai_object_id_t port_id,
        _In_ const sai_port_stat_counter_t *counter_ids,
        _In_ uint32_t number_of_counters,
        _Out_ uint64_t* counters)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

/**
 * Routine Description:
 *   @brief Clear port statistics counters.
 *
 * Arguments:
 *    @param[in] port_id - port id
 *    @param[in] counter_ids - specifies the array of counter ids
 *    @param[in] number_of_counters - number of counters in the array
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_clear_port_stats(
        _In_ sai_object_id_t port_id,
        _In_ const sai_port_stat_counter_t *counter_ids,
        _In_ uint32_t number_of_counters)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

/**
 * Routine Description:
 *   @brief Clear port's all statistics counters.
 *
 * Arguments:
 *    @param[in] port_id - port id
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_clear_port_all_stats(
        _In_ sai_object_id_t port_id)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

/**
 * @brief Port methods table retrieved with sai_api_query()
 */
const sai_port_api_t redis_port_api = {
    redis_set_port_attribute,
    redis_get_port_attribute,
    redis_get_port_stats,
    redis_clear_port_stats,
    redis_clear_port_all_stats,
};
