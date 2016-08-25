#include "sai_redis.h"

std::set<sai_object_id_t> local_buffer_pools_set;
std::set<sai_object_id_t> local_buffer_profiles_set;

/**
 * @brief Set ingress priority group attribute
 * @param[in] ingress_pg_id ingress priority group id
 * @param[in] attr attribute to set
 *
 * @return  SAI_STATUS_SUCCESS on success
 *           Failure status code on error
 */
sai_status_t redis_set_ingress_priority_group_attr(
    _In_ sai_object_id_t ingress_pg_id,
    _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    sai_status_t status = redis_generic_set(
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
sai_status_t redis_get_ingress_priority_group_attr(
    _In_ sai_object_id_t ingress_pg_id,
    _In_ uint32_t attr_count,
    _Inout_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    sai_status_t status = redis_generic_get(
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
sai_status_t redis_get_ingress_priority_group_stats(
    _In_ sai_object_id_t ingress_pg_id,
    _In_ const sai_ingress_priority_group_stat_counter_t *counter_ids,
    _In_ uint32_t number_of_counters,
    _Out_ uint64_t* counters)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

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
sai_status_t redis_clear_ingress_priority_group_stats(
    _In_ sai_object_id_t ingress_pg_id,
    _In_ const sai_ingress_priority_group_stat_counter_t *counter_ids,
    _In_ uint32_t number_of_counters)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

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
sai_status_t redis_create_buffer_pool(
    _Out_ sai_object_id_t* pool_id,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr_list == NULL)
    {
        SWSS_LOG_ERROR("attribute list parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_count < 2)
    {
        SWSS_LOG_ERROR("attribute count must be at least 2");

        // SAI_BUFFER_POOL_ATTR_TYPE
        // SAI_BUFFER_POOL_ATTR_SIZE

        return SAI_STATUS_INVALID_PARAMETER;
    }

    const sai_attribute_t* attr_type = redis_get_attribute_by_id(SAI_BUFFER_POOL_ATTR_TYPE, attr_count, attr_list);
    const sai_attribute_t* attr_size = redis_get_attribute_by_id(SAI_BUFFER_POOL_ATTR_SIZE, attr_count, attr_list);

    const sai_attribute_t* attr_th_mode = redis_get_attribute_by_id(SAI_BUFFER_POOL_ATTR_TH_MODE, attr_count, attr_list);

    if (attr_type == NULL)
    {
        SWSS_LOG_ERROR("missing type attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    sai_buffer_pool_type_t type = (sai_buffer_pool_type_t)attr_type->value.s32;

    switch (type)
    {
        case SAI_BUFFER_POOL_INGRESS:
        case SAI_BUFFER_POOL_EGRESS:
            // ok
            break;

        default:

            SWSS_LOG_ERROR("invalid type attribute value: %d", type);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_size == NULL)
    {
        SWSS_LOG_ERROR("missing size attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    // TODO additional logic can be required for size check

    sai_buffer_threshold_mode_t th_mode = SAI_BUFFER_THRESHOLD_MODE_DYNAMIC; // default value

    if (attr_th_mode != NULL)
    {
        th_mode = (sai_buffer_threshold_mode_t)attr_th_mode->value.s32;
    }

    switch (th_mode)
    {
        case SAI_BUFFER_THRESHOLD_MODE_STATIC:
        case SAI_BUFFER_THRESHOLD_MODE_DYNAMIC:
            // ok
            break;

        default:

            SWSS_LOG_ERROR("invalid threshold type value: %d", th_mode);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_create(
        SAI_OBJECT_TYPE_BUFFER_POOL,
        pool_id,
        attr_count,
        attr_list);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("inserting pool_id %llx to local state", *pool_id);

        local_buffer_pools_set.insert(*pool_id);

        // TODO increase reference count for used object ids
    }

    return status;
}

/**
 * @brief Remove buffer pool
 * @param[in] pool_id buffer pool id
 * @return SAI_STATUS_SUCCESS on success
 *           Failure status code on error
 */
sai_status_t redis_remove_buffer_pool(
    _In_ sai_object_id_t pool_id)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    // TODO check if it safe to remove buffer pool
    // used by buffer profile

    if (local_buffer_pools_set.find(pool_id) == local_buffer_pools_set.end())
    {
        SWSS_LOG_ERROR("buffer pool %llx is missing", pool_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_remove(
            SAI_OBJECT_TYPE_BUFFER_POOL,
            pool_id);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("erasing buffer pool %llx from local state", pool_id);

        local_buffer_pools_set.erase(pool_id);
    }

    return status;
}

/**
 * @brief Set buffer pool attribute
 * @param[in] pool_id buffer pool id
 * @param[in] attr attribute
 * @return SAI_STATUS_SUCCESS on success
 *           Failure status code on error
 */
sai_status_t redis_set_buffer_pool_attr(
    _In_ sai_object_id_t pool_id,
    _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr == NULL)
    {
        SWSS_LOG_ERROR("attribute parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (local_buffer_pools_set.find(pool_id) == local_buffer_pools_set.end())
    {
        SWSS_LOG_ERROR("buffer pool %llx is missing", pool_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    switch (attr->id)
    {
        case SAI_BUFFER_POOL_ATTR_SIZE:
            break;

        default:

            SWSS_LOG_ERROR("setting attribute id %d is not supported", attr->id);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_set(
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
sai_status_t redis_get_buffer_pool_attr(
    _In_ sai_object_id_t pool_id,
    _In_ uint32_t attr_count,
    _Inout_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr_list == NULL)
    {
        SWSS_LOG_ERROR("attribute list parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_count < 1)
    {
        SWSS_LOG_ERROR("attribute count must be at least 1");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (local_buffer_pools_set.find(pool_id) == local_buffer_pools_set.end())
    {
        SWSS_LOG_ERROR("buffer pool %llx is missing", pool_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        sai_attribute_t* attr = &attr_list[i];

        switch (attr->id)
        {
            case SAI_BUFFER_POOL_ATTR_SHARED_SIZE:
            case SAI_BUFFER_POOL_ATTR_TYPE:
            case SAI_BUFFER_POOL_ATTR_SIZE:
            case SAI_BUFFER_POOL_ATTR_TH_MODE:
                // ok
                break;

            default:

                SWSS_LOG_ERROR("getting attribute id %d is not supported", attr->id);

                return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    sai_status_t status = redis_generic_get(
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
sai_status_t redis_get_buffer_pool_stats(
    _In_ sai_object_id_t pool_id,
    _In_ const sai_buffer_pool_stat_counter_t *counter_ids,
    _In_ uint32_t number_of_counters,
    _Out_ uint64_t* counters)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

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
sai_status_t redis_create_buffer_profile(
    _Out_ sai_object_id_t* buffer_profile_id,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr_list == NULL)
    {
        SWSS_LOG_ERROR("attribute list parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_count < 2)
    {
        SWSS_LOG_ERROR("attribute count must be at least 2");

        // SAI_BUFFER_PROFILE_ATTR_POOL_ID
        // SAI_BUFFER_PROFILE_ATTR_BUFFER_SIZE

        return SAI_STATUS_INVALID_PARAMETER;
    }

    const sai_attribute_t* attr_pool_id = redis_get_attribute_by_id(SAI_BUFFER_PROFILE_ATTR_POOL_ID, attr_count, attr_list);
    const sai_attribute_t* attr_buffer_size = redis_get_attribute_by_id(SAI_BUFFER_PROFILE_ATTR_BUFFER_SIZE, attr_count, attr_list);

    if (attr_pool_id == NULL)
    {
        SWSS_LOG_ERROR("missing pool id attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    sai_object_id_t pool_id = attr_pool_id->value.oid;

    if (local_buffer_pools_set.find(pool_id) == local_buffer_pools_set.end())
    {
        SWSS_LOG_ERROR("pool id %llx is missing", pool_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_buffer_size == NULL)
    {
        SWSS_LOG_ERROR("missing buffer size attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    // TODO extra logic on checking profile buffer size may be needed

    const sai_attribute_t* attr_th_mode = redis_get_attribute_by_id(SAI_BUFFER_PROFILE_ATTR_TH_MODE, attr_count, attr_list);

    sai_buffer_threshold_mode_t th_mode = SAI_BUFFER_THRESHOLD_MODE_DYNAMIC; // default value NEEDS TO BE CHANGED to DEFAULT enum

    if (attr_th_mode != NULL)
    {
        th_mode = (sai_buffer_threshold_mode_t)attr_th_mode->value.s32;
    }

    switch (th_mode)
    {
        case SAI_BUFFER_THRESHOLD_MODE_STATIC:
        case SAI_BUFFER_THRESHOLD_MODE_DYNAMIC:
            // ok
            break;

        default:

            SWSS_LOG_ERROR("invalid threshold type value: %d", th_mode);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    // TODO we need to query other attribute pool id assigned and check wheter mode is dynamic/static

    const sai_attribute_t* attr_shared_dynamic_th = redis_get_attribute_by_id(SAI_BUFFER_PROFILE_ATTR_SHARED_DYNAMIC_TH, attr_count, attr_list);

    if (attr_shared_dynamic_th != NULL)
    {
        // TODO check size
    }

    const sai_attribute_t* attr_shared_static_th = redis_get_attribute_by_id(SAI_BUFFER_PROFILE_ATTR_SHARED_STATIC_TH, attr_count, attr_list);

    if (attr_shared_static_th != NULL)
    {
        // TODO check size
    }

    // TODO additional logic can be required for size check
    // TODO check xoff xon thresholds

    sai_status_t status = redis_generic_create(
            SAI_OBJECT_TYPE_BUFFER_PROFILE,
            buffer_profile_id,
            attr_count,
            attr_list);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("inserting pool_id %llx to local state", *buffer_profile_id);

        local_buffer_profiles_set.insert(*buffer_profile_id);

        // TODO increase reference count for used object ids
    }

    return status;
}

/**
 * @brief Remove buffer profile
 * @param[in] buffer_profile_id buffer profile id
 * @return SAI_STATUS_SUCCESS on success
 *           Failure status code on error
 */
sai_status_t redis_remove_buffer_profile(
    _In_ sai_object_id_t buffer_profile_id)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    // TODO check if its ok to remove profile

    if (local_buffer_profiles_set.find(buffer_profile_id) == local_buffer_profiles_set.end())
    {
        SWSS_LOG_ERROR("buffer profile %llx is missing", buffer_profile_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_remove(
            SAI_OBJECT_TYPE_BUFFER_PROFILE,
            buffer_profile_id);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("erasing buffer profile %llx from local state", buffer_profile_id);

        local_buffer_profiles_set.erase(buffer_profile_id);
    }

    return status;
}

/**
 * @brief Set buffer profile attribute
 * @param[in] buffer_profile_id buffer profile id
 * @param[in] attr attribute
 * @return SAI_STATUS_SUCCESS on success
 *           Failure status code on error
 */
sai_status_t redis_set_buffer_profile_attr(
    _In_ sai_object_id_t buffer_profile_id,
    _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    // TODO on set, changing buffer pool on profiles should noe be possible
    // to change dynamic to static on the fly
    // pool_id on profile should be create_only ?

    if (attr == NULL)
    {
        SWSS_LOG_ERROR("attribute parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (local_buffer_profiles_set.find(buffer_profile_id) == local_buffer_profiles_set.end())
    {
        SWSS_LOG_ERROR("buffer profile %llx is missing", buffer_profile_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    switch (attr->id)
    {
        case SAI_BUFFER_PROFILE_ATTR_POOL_ID:
        case SAI_BUFFER_PROFILE_ATTR_BUFFER_SIZE:
        case SAI_BUFFER_PROFILE_ATTR_TH_MODE:

            // TODO setting those may be dependend of other attribute
        case SAI_BUFFER_PROFILE_ATTR_SHARED_DYNAMIC_TH:
        case SAI_BUFFER_PROFILE_ATTR_SHARED_STATIC_TH:
        case SAI_BUFFER_PROFILE_ATTR_XOFF_TH:
        case SAI_BUFFER_PROFILE_ATTR_XON_TH:
        default:

            SWSS_LOG_ERROR("setting attribute id %d is not supported", attr->id);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_set(
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
sai_status_t redis_get_buffer_profile_attr(
    _In_ sai_object_id_t buffer_profile_id,
    _In_ uint32_t attr_count,
    _Inout_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr_list == NULL)
    {
        SWSS_LOG_ERROR("attribute list parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_count < 1)
    {
        SWSS_LOG_ERROR("attribute count must be at least 1");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (local_buffer_profiles_set.find(buffer_profile_id) == local_buffer_profiles_set.end())
    {
        SWSS_LOG_ERROR("buffer profile %llx is missing", buffer_profile_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        sai_attribute_t* attr = &attr_list[i];

        switch (attr->id)
        {
            case SAI_BUFFER_PROFILE_ATTR_POOL_ID:
            case SAI_BUFFER_PROFILE_ATTR_BUFFER_SIZE: // TODO add other attributes for get
                // ok
                break;

            default:

                SWSS_LOG_ERROR("getting attribute id %d is not supported", attr->id);

                return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    sai_status_t status = redis_generic_get(
            SAI_OBJECT_TYPE_BUFFER_PROFILE,
            buffer_profile_id,
            attr_count,
            attr_list);

    return status;
}

/**
 *  @brief buffer methods table retrieved with sai_api_query()
 */
const sai_buffer_api_t redis_buffer_api = {
    redis_create_buffer_pool,
    redis_remove_buffer_pool,
    redis_set_buffer_pool_attr,
    redis_get_buffer_pool_attr,
    redis_get_buffer_pool_stats,

    redis_set_ingress_priority_group_attr,
    redis_get_ingress_priority_group_attr,
    redis_get_ingress_priority_group_stats,
    redis_clear_ingress_priority_group_stats,

    redis_create_buffer_profile,
    redis_remove_buffer_profile,
    redis_set_buffer_profile_attr,
    redis_get_buffer_profile_attr,
};
