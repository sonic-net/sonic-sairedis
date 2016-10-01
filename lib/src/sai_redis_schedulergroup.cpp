#include "sai_redis.h"

/**
 * @brief Create Scheduler group
 *
 * @param[out] scheduler_group_id Scheudler group id
 * @param[in] attr_count number of attributes
 * @param[in] attr_list array of attributes
 *
 * @return SAI_STATUS_SUCCESS on success
 *          Failure status code on error
 */
sai_status_t redis_create_scheduler_group(
        _Out_ sai_object_id_t *scheduler_group_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_create_oid(
            SAI_OBJECT_TYPE_SCHEDULER_GROUP,
            scheduler_group_id,
            attr_count,
            attr_list,
            &redis_generic_create);
}

/**
 * @brief Remove Scheduler group
 *
 * @param[in] scheduler_group_id Scheudler group id
 *
 * @return SAI_STATUS_SUCCESS on success
 *          Failure status code on error
 */
sai_status_t redis_remove_scheduler_group(
        _In_ sai_object_id_t scheduler_group_id)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_remove_oid(
            SAI_OBJECT_TYPE_SCHEDULER_GROUP,
            scheduler_group_id,
            &redis_generic_remove);
}

/**
 * @brief Set Scheduler group Attribute
 *
 * @param[in] scheduler_group_id Scheudler group id
 * @param[in] attr attribute to set
 *
 * @return SAI_STATUS_SUCCESS on success
 *          Failure status code on error
 */
sai_status_t redis_set_scheduler_group_attribute(
        _In_ sai_object_id_t scheduler_group_id,
        _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_set_oid(
            SAI_OBJECT_TYPE_SCHEDULER_GROUP,
            scheduler_group_id,
            attr,
            &redis_generic_set);
}

/**
 * @brief Get Scheduler Group attribute
 *
 * @param[in] scheduler_group_id - scheduler group id
 * @param[in] attr_count - number of attributes
 * @param[inout] attr_list - array of attributes
 *
 * @return SAI_STATUS_SUCCESS on success
 *        Failure status code on error
 */

sai_status_t redis_get_scheduler_group_attribute(
        _In_ sai_object_id_t scheduler_group_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return meta_sai_get_oid(
            SAI_OBJECT_TYPE_SCHEDULER_GROUP,
            scheduler_group_id,
            attr_count,
            attr_list,
            &redis_generic_get);
}

/**
 * @brief   Add Child queue/group objects to scheduler group
 *
 * @param[in] scheduler_group_id Scheduler group id.
 * @param[in] child_count number of child count
 * @param[in] child_objects array of child objects
 *
 * @return SAI_STATUS_SUCCESS on success
 *        Failure status code on error
 */
sai_status_t redis_add_child_object_to_group(
        _In_ sai_object_id_t scheduler_group_id,
        _In_ uint32_t child_count,
        _In_ const sai_object_id_t* child_objects)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

/**
 * @brief   Remove Child queue/group objects from scheduler group
 *
 * @param[in] scheduler_group_id Scheduler group id.
 * @param[in] child_count number of child count
 * @param[in] child_objects array of child objects
 *
 * @return SAI_STATUS_SUCCESS on success
 *        Failure status code on error
 */
sai_status_t redis_remove_child_object_from_group(
        _In_ sai_object_id_t scheduler_group_id,
        _In_ uint32_t child_count,
        _In_ const sai_object_id_t* child_objects)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

/**
 * @brief Scheduler Group methods table retrieved with sai_api_query()
 */
const sai_scheduler_group_api_t redis_scheduler_group_api = {
    redis_create_scheduler_group,
    redis_remove_scheduler_group,
    redis_set_scheduler_group_attribute,
    redis_get_scheduler_group_attribute,
    redis_add_child_object_to_group,
    redis_remove_child_object_from_group,
};
