#include "sai_redis.h"

/**
 * Routine Description:
 *    @brief Create a VLAN
 *
 * Arguments:
 *    @param[in] vlan_id - VLAN id
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_create_vlan(
        _In_ sai_vlan_id_t vlan_id)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_create_vlan(
            vlan_id,
            &redis_generic_create_vlan);
}

/**
 * Routine Description:
 *    @brief Remove a VLAN
 *
 * Arguments:
 *    @param[in] vlan_id - VLAN id
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_remove_vlan(
        _In_ sai_vlan_id_t vlan_id)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_remove_vlan(
            vlan_id,
            &redis_generic_remove_vlan);
}

/**
 * Routine Description:
 *    @brief Set VLAN attribute Value
 *
 * Arguments:
 *    @param[in] vlan_id - VLAN id
 *    @param[in] attr - attribute
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_set_vlan_attribute(
        _In_ sai_vlan_id_t vlan_id,
        _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_set_vlan(
            vlan_id,
            attr,
            &redis_generic_set_vlan);
}

/**
 * Routine Description:
 *    @brief Get VLAN attribute Value
 *
 * Arguments:
 *    @param[in] vlan_id - VLAN id
 *    @param[in] attr_count - number of attributes
 *    @param[inout] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_get_vlan_attribute(
        _In_ sai_vlan_id_t vlan_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_get_vlan(
            vlan_id,
            attr_count,
            attr_list,
            &redis_generic_get_vlan);
}

/**
 * Routine Description:
 *    @brief Create VLAN member
 *
 * Arguments:
 *    @param[out] vlan_member_id - VLAN member id
 *    @param[in] attr_count - number of attributes
 *    @param[in] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_create_vlan_member(
        _Out_ sai_object_id_t* vlan_member_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_create_oid(
            SAI_OBJECT_TYPE_VLAN_MEMBER,
            vlan_member_id,
            attr_count,
            attr_list,
            &redis_generic_create);
}

/**
 * Routine Description:
 *    @brief Remove VLAN member
 *
 * Arguments:
 *    @param[in] vlan_member_id - VLAN member id
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_remove_vlan_member(
        _In_ sai_object_id_t vlan_member_id)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_remove_oid(
            SAI_OBJECT_TYPE_VLAN_MEMBER,
            vlan_member_id,
            &redis_generic_remove);
}

/**
 * Routine Description:
 *    @brief Set VLAN member attribute
 *
 * Arguments:
 *    @param[in] vlan_member_id - VLAN member id
 *    @param[in] attr - Structure containing ID and value to be set
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_set_vlan_member_attribute(
        _In_ sai_object_id_t vlan_member_id,
        _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_set_oid(
            SAI_OBJECT_TYPE_VLAN_MEMBER,
            vlan_member_id,
            attr,
            &redis_generic_set);
}

/**
 * Routine Description:
 *    @brief Get VLAN member attribute
 *
 * Arguments:
 *    @param[in] vlan_member_id - VLAN member id
 *    @param[in] attr_count - number of attributes
 *    @param[inout] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_get_vlan_member_attribute(
        _In_ sai_object_id_t vlan_member_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_get_oid(
            SAI_OBJECT_TYPE_VLAN_MEMBER,
            vlan_member_id,
            attr_count,
            attr_list,
            &redis_generic_get);
}

/**
 * Routine Description:
 *   @brief Get vlan statistics counters.
 *
 * Arguments:
 *    @param[in] vlan_id - VLAN id
 *    @param[in] counter_ids - specifies the array of counter ids
 *    @param[in] number_of_counters - number of counters in the array
 *    @param[out] counters - array of resulting counter values.
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_get_vlan_stats(
        _In_ sai_vlan_id_t vlan_id,
        _In_ const sai_vlan_stat_counter_t *counter_ids,
        _In_ uint32_t number_of_counters,
        _Out_ uint64_t* counters)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

/**
 * Routine Description:
 *   @brief Clear vlan statistics counters.
 *
 * Arguments:
 *    @param[in] vlan_id - vlan id
 *    @param[in] counter_ids - specifies the array of counter ids
 *    @param[in] number_of_counters - number of counters in the array
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_clear_vlan_stats(
        _In_ sai_vlan_id_t vlan_id,
        _In_ const sai_vlan_stat_counter_t *counter_ids,
        _In_ uint32_t number_of_counters)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

/**
 * @brief VLAN methods table retrieved with sai_api_query()
 */
const sai_vlan_api_t redis_vlan_api = {
    redis_create_vlan,
    redis_remove_vlan,
    redis_set_vlan_attribute,
    redis_get_vlan_attribute,
    redis_create_vlan_member,
    redis_remove_vlan_member,
    redis_set_vlan_member_attribute,
    redis_get_vlan_member_attribute,
    redis_get_vlan_stats,
    redis_clear_vlan_stats,
};
