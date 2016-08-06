#include "sai_vs.h"

/**
 * @brief Set ingress priority group attribute
 * @param[in] ingress_pg_id ingress priority group id
 * @param[in] attr attribute to set
 *
 * @return  SAI_STATUS_SUCCESS on success
 *           Failure status code on error
 */
sai_status_t vs_set_ingress_priority_group_attr(
    _In_ sai_object_id_t ingress_pg_id,
    _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_set(
            SAI_OBJECT_TYPE_PRIORITY_GROUP,
            ingress_pg_id,
            attr);

    return status;
}

/**
 * @brief Get ingress priority group attributes
 * @param[in] ingress_pg_id ingress priority group id
 * @param[in] attr_count number of attributes
 * @param[inout] attr_list array of attributes
 *
 * @return  SAI_STATUS_SUCCESS on success
 *           Failure status code on error
 */
sai_status_t vs_get_ingress_priority_group_attr(
    _In_ sai_object_id_t ingress_pg_id,
    _In_ uint32_t attr_count,
    _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_get(
            SAI_OBJECT_TYPE_PRIORITY_GROUP,
            ingress_pg_id,
            attr_count,
            attr_list);

    return status;
}

/**
* @brief   Get ingress priority group statistics counters.
*
* @param[in] ingress_pg_id ingress priority group id
* @param[in] counter_ids specifies the array of counter ids
* @param[in] number_of_counters number of counters in the array
* @param[out] counters array of resulting counter values.
*
* @return SAI_STATUS_SUCCESS on success
*         Failure status code on error
*/
sai_status_t vs_get_ingress_priority_group_stats(
    _In_ sai_object_id_t ingress_pg_id,
    _In_ const sai_ingress_priority_group_stat_counter_t *counter_ids,
    _In_ uint32_t number_of_counters,
    _Out_ uint64_t* counters)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

/**
* @brief   Clear ingress priority group statistics counters.
*
* @param[in] ingress_pg_id ingress priority group id
* @param[in] counter_ids specifies the array of counter ids
* @param[in] number_of_counters number of counters in the array
*
* @return SAI_STATUS_SUCCESS on success
*         Failure status code on error
*/
sai_status_t vs_clear_ingress_priority_group_stats(
    _In_ sai_object_id_t ingress_pg_id,
    _In_ const sai_ingress_priority_group_stat_counter_t *counter_ids,
    _In_ uint32_t number_of_counters)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

/**
 * @brief Create buffer pool
 * @param[out] pool_id buffer pool id
 * @param[in] attr_count number of attributes
 * @param[in] attr_list array of attributes
 * @return SAI_STATUS_SUCCESS on success
 *           Failure status code on error
 */
sai_status_t vs_create_buffer_pool(
    _Out_ sai_object_id_t* pool_id,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_create(
        SAI_OBJECT_TYPE_BUFFER_POOL,
        pool_id,
        attr_count,
        attr_list);

    return status;
}

/**
 * @brief Remove buffer pool
 * @param[in] pool_id buffer pool id
 * @return SAI_STATUS_SUCCESS on success
 *           Failure status code on error
 */
sai_status_t vs_remove_buffer_pool(
    _In_ sai_object_id_t pool_id)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_remove(
            SAI_OBJECT_TYPE_BUFFER_POOL,
            pool_id);

    return status;
}

/**
 * @brief Set buffer pool attribute
 * @param[in] pool_id buffer pool id
 * @param[in] attr attribute
 * @return SAI_STATUS_SUCCESS on success
 *           Failure status code on error
 */
sai_status_t vs_set_buffer_pool_attr(
    _In_ sai_object_id_t pool_id,
    _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_set(
            SAI_OBJECT_TYPE_BUFFER_POOL,
            pool_id,
            attr);

    return status;
}

/**
 * @brief Get buffer pool attributes
 * @param[in] pool_id buffer pool id
 * @param[in] attr_count number of attributes
 * @param[inout] attr_list array of attributes
 * @return SAI_STATUS_SUCCESS on success
 *           Failure status code on error
 */
sai_status_t vs_get_buffer_pool_attr(
    _In_ sai_object_id_t pool_id,
    _In_ uint32_t attr_count,
    _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_get(
            SAI_OBJECT_TYPE_BUFFER_POOL,
            pool_id,
            attr_count,
            attr_list);

    return status;
}

/**
* @brief   Get buffer pool statistics counters.
*
* @param[in] pool_id buffer pool id
* @param[in] counter_ids specifies the array of counter ids
* @param[in] number_of_counters number of counters in the array
* @param[out] counters array of resulting counter values.
*
* @return SAI_STATUS_SUCCESS on success
*         Failure status code on error
*/
sai_status_t vs_get_buffer_pool_stats(
    _In_ sai_object_id_t pool_id,
    _In_ const sai_buffer_pool_stat_counter_t *counter_ids,
    _In_ uint32_t number_of_counters,
    _Out_ uint64_t* counters)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

/**
 * @brief Create buffer profile
 * @param[out] buffer_profile_id buffer profile id
 * @param[in] attr_count number of attributes
 * @param[in] attr_list array of attributes
 * @return SAI_STATUS_SUCCESS on success
 *           Failure status code on error
 */
sai_status_t vs_create_buffer_profile(
    _Out_ sai_object_id_t* buffer_profile_id,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_create(
            SAI_OBJECT_TYPE_BUFFER_PROFILE,
            buffer_profile_id,
            attr_count,
            attr_list);

    return status;
}

/**
 * @brief Remove buffer profile
 * @param[in] buffer_profile_id buffer profile id
 * @return SAI_STATUS_SUCCESS on success
 *           Failure status code on error
 */
sai_status_t vs_remove_buffer_profile(
    _In_ sai_object_id_t buffer_profile_id)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_remove(
            SAI_OBJECT_TYPE_BUFFER_PROFILE,
            buffer_profile_id);

    return status;
}

/**
 * @brief Set buffer profile attribute
 * @param[in] buffer_profile_id buffer profile id
 * @param[in] attr attribute
 * @return SAI_STATUS_SUCCESS on success
 *           Failure status code on error
 */
sai_status_t vs_set_buffer_profile_attr(
    _In_ sai_object_id_t buffer_profile_id,
    _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_set(
            SAI_OBJECT_TYPE_BUFFER_PROFILE,
            buffer_profile_id,
            attr);

    return status;
}

/**
 * @brief Get buffer profile attributes
 * @param[in] buffer_profile_id buffer profile id
 * @param[in] attr_count number of attributes
 * @param[inout] attr_list array of attributes
 * @return SAI_STATUS_SUCCESS on success
 *           Failure status code on error
 */
sai_status_t vs_get_buffer_profile_attr(
    _In_ sai_object_id_t buffer_profile_id,
    _In_ uint32_t attr_count,
    _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_status_t status = vs_generic_get(
            SAI_OBJECT_TYPE_BUFFER_PROFILE,
            buffer_profile_id,
            attr_count,
            attr_list);

    return status;
}

/**
 *  @brief buffer methods table retrieved with sai_api_query()
 */
const sai_buffer_api_t vs_buffer_api = {
    vs_create_buffer_pool,
    vs_remove_buffer_pool,
    vs_set_buffer_pool_attr,
    vs_get_buffer_pool_attr,
    vs_get_buffer_pool_stats,

    vs_set_ingress_priority_group_attr,
    vs_get_ingress_priority_group_attr,
    vs_get_ingress_priority_group_stats,
    vs_clear_ingress_priority_group_stats,

    vs_create_buffer_profile,
    vs_remove_buffer_profile,
    vs_set_buffer_profile_attr,
    vs_get_buffer_profile_attr,
};
