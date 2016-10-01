#include "sai_redis.h"

/**
 * Routine Description:
 *    @brief Create next hop group
 *
 * Arguments:
 *    @param[out] next_hop_group_id - next hop group id
 *    @param[in] attr_count - number of attributes
 *    @param[in] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_create_next_hop_group(
        _Out_ sai_object_id_t* next_hop_group_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_create_oid(
            SAI_OBJECT_TYPE_NEXT_HOP_GROUP,
            next_hop_group_id,
            attr_count,
            attr_list,
            &redis_generic_create);
}

/**
 * Routine Description:
 *    @brief Remove next hop group
 *
 * Arguments:
 *    @param[in] next_hop_group_id - next hop group id
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_remove_next_hop_group(
        _In_ sai_object_id_t next_hop_group_id)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_remove_oid(
            SAI_OBJECT_TYPE_NEXT_HOP_GROUP,
            next_hop_group_id,
            &redis_generic_remove);
}

/**
 * Routine Description:
 *    @brief Set Next Hop Group attribute
 *
 * Arguments:
 *    @param[in] sai_object_id_t - next_hop_group_id
 *    @param[in] attr - attribute
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_set_next_hop_group_attribute(
        _In_ sai_object_id_t next_hop_group_id,
        _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_set_oid(
            SAI_OBJECT_TYPE_NEXT_HOP_GROUP,
            next_hop_group_id,
            attr,
            &redis_generic_set);
}

/**
 * Routine Description:
 *    @brief Get Next Hop Group attribute
 *
 * Arguments:
 *    @param[in] sai_object_id_t - next_hop_group_id
 *    @param[in] attr_count - number of attributes
 *    @param[inout] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_get_next_hop_group_attribute(
        _In_ sai_object_id_t next_hop_group_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_get_oid(
            SAI_OBJECT_TYPE_NEXT_HOP_GROUP,
            next_hop_group_id,
            attr_count,
            attr_list,
            &redis_generic_get);
}

/**
 * Routine Description:
 *    @brief Add next hop to a group
 *
 * Arguments:
 *    @param[in] next_hop_group_id - next hop group id
 *    @param[in] next_hop_count - number of next hops
 *    @param[in] nexthops - array of next hops
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_add_next_hop_to_group(
        _In_ sai_object_id_t next_hop_group_id,
        _In_ uint32_t next_hop_count,
        _In_ const sai_object_id_t* nexthops)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

/**
 * Routine Description:
 *    @brief Remove next hop from a group
 *
 * Arguments:
 *    @param[in] next_hop_group_id - next hop group id
 *    @param[in] next_hop_count - number of next hops
 *    @param[in] nexthops - array of next hops
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_remove_next_hop_from_group(
        _In_ sai_object_id_t next_hop_group_id,
        _In_ uint32_t next_hop_count,
        _In_ const sai_object_id_t* nexthops)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

/**
 * @brief Next Hop methods table retrieved with sai_api_query()
 */
const sai_next_hop_group_api_t redis_next_hop_group_api = {
    redis_create_next_hop_group,
    redis_remove_next_hop_group,
    redis_set_next_hop_group_attribute,
    redis_get_next_hop_group_attribute,
    redis_add_next_hop_to_group,
    redis_remove_next_hop_from_group,
};
