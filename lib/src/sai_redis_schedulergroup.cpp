#include "sai_redis.h"

std::set<sai_object_id_t> local_scheduler_groups_set;

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

    if (attr_list == NULL)
    {
        SWSS_LOG_ERROR("attribute list parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_count < 3)
    {
        SWSS_LOG_ERROR("attribute count must be at least 3");

        // SAI_SCHEDULER_GROUP_ATTR_PORT_ID
        // SAI_SCHEDULER_GROUP_ATTR_LEVEL
        // SAI_SCHEDULER_GROUP_ATTR_MAX_CHILDS

        return SAI_STATUS_INVALID_PARAMETER;
    }

    const sai_attribute_t* attr_port_id = redis_get_attribute_by_id(SAI_SCHEDULER_GROUP_ATTR_PORT_ID, attr_count, attr_list);
    const sai_attribute_t* attr_level = redis_get_attribute_by_id(SAI_SCHEDULER_GROUP_ATTR_LEVEL, attr_count, attr_list);
    const sai_attribute_t* attr_max_childs = redis_get_attribute_by_id(SAI_SCHEDULER_GROUP_ATTR_MAX_CHILDS , attr_count, attr_list);

    if (attr_port_id == NULL)
    {
        SWSS_LOG_ERROR("missing type attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    sai_object_id_t port_id = attr_port_id->value.oid;

    // TODO port only or lag? tunnel ?

    if (local_ports_set.find(port_id) == local_ports_set.end())
    {
        SWSS_LOG_ERROR("port %llx is missing", port_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_level == NULL)
    {
        SWSS_LOG_ERROR("missing level attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    uint8_t level = attr_level->value.u8;

    if (level > 16)
    {
        SWSS_LOG_ERROR("invalid level value: %u <%u..%u>", level, 0, 16);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // TODO level will require some additional validation to not crete loops

    if (attr_max_childs == NULL)
    {
        SWSS_LOG_ERROR("missing max childs attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    uint8_t max_childs = attr_max_childs->value.u8;

    if (max_childs > 64)
    {
        SWSS_LOG_ERROR("invalid max childs value: %u <%u..%u>", max_childs, 0, 64);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // TODO max childs may require more validation

    sai_status_t status = redis_generic_create(
            SAI_OBJECT_TYPE_SCHEDULER_GROUP,
            scheduler_group_id,
            attr_count,
            attr_list);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("inserting scheduler group %llx to local state", *scheduler_group_id);

        local_scheduler_groups_set.insert(*scheduler_group_id);
    }

    return status;
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

    // TODO check if it safe to remove scheduler group
    // extra validation can be required here since there
    // is a lot of dependencies

    if (local_scheduler_groups_set.find(scheduler_group_id) == local_scheduler_groups_set.end())
    {
        SWSS_LOG_ERROR("scheduler group %llx is missing", scheduler_group_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_remove(
            SAI_OBJECT_TYPE_SCHEDULER_GROUP,
            scheduler_group_id);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("erasing scheduler group %llx from local state", scheduler_group_id);

        local_scheduler_groups_set.erase(scheduler_group_id);
    }

    return status;
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

    if (attr == NULL)
    {
        SWSS_LOG_ERROR("attribute parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (local_scheduler_groups_set.find(scheduler_group_id) == local_scheduler_groups_set.end())
    {
        SWSS_LOG_ERROR("scheduler group %llx is missing", scheduler_group_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    switch (attr->id)
    {
        // currently we don't allow to set any attributes
        default:

            SWSS_LOG_ERROR("setting attribute id %d is not supported", attr->id);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_set(
            SAI_OBJECT_TYPE_SCHEDULER_GROUP,
            scheduler_group_id,
            attr);

    return status;
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

    if (local_scheduler_groups_set.find(scheduler_group_id) == local_scheduler_groups_set.end())
    {
        SWSS_LOG_ERROR("scheduler group %llx is missing", scheduler_group_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        sai_attribute_t* attr = &attr_list[i];

        switch (attr->id)
        {
            case SAI_SCHEDULER_GROUP_ATTR_CHILD_COUNT:
            case SAI_SCHEDULER_GROUP_ATTR_CHILD_LIST:

            case SAI_SCHEDULER_GROUP_ATTR_PORT_ID:
            case SAI_SCHEDULER_GROUP_ATTR_LEVEL:
            case SAI_SCHEDULER_GROUP_ATTR_MAX_CHILDS:
                // ok
                break;

            default:

                SWSS_LOG_ERROR("getting attribute id %d is not supported", attr->id);

                return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    sai_status_t status = redis_generic_get(
            SAI_OBJECT_TYPE_SCHEDULER_GROUP,
            scheduler_group_id,
            attr_count,
            attr_list);

    return status;
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

    // TODO must be converted to quad API
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

    // TODO must be converted to quad API
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
