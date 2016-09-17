#include "sai_redis.h"

std::set<sai_object_id_t> local_next_hop_groups_set;

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

    if (attr_list == NULL)
    {
        SWSS_LOG_ERROR("attribute list parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_count < 2)
    {
        SWSS_LOG_ERROR("attribute count must be at least 2");

        // SAI_NEXT_HOP_GROUP_ATTR_TYPE
        // SAI_NEXT_HOP_GROUP_ATTR_NEXT_HOP_LIST

        return SAI_STATUS_INVALID_PARAMETER;
    }

    const sai_attribute_t* attr_type = redis_get_attribute_by_id(SAI_NEXT_HOP_GROUP_ATTR_TYPE, attr_count, attr_list);
    const sai_attribute_t* attr_next_hop_list = redis_get_attribute_by_id(SAI_NEXT_HOP_GROUP_ATTR_NEXT_HOP_LIST, attr_count, attr_list);

    if (attr_type == NULL)
    {
        SWSS_LOG_ERROR("missing type attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    sai_next_hop_group_type_t type = (sai_next_hop_group_type_t)attr_type->value.s32;

    switch (type)
    {
        case SAI_NEXT_HOP_GROUP_ECMP:
            // ok
            break;

        default:

            SWSS_LOG_ERROR("invalid type attribute value: %d", type);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_next_hop_list == NULL)
    {
        SWSS_LOG_ERROR("missing next hop list attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    sai_object_list_t next_hop_list = attr_next_hop_list->value.objlist;

    if (next_hop_list.count < 1)
    {
        SWSS_LOG_ERROR("next hop list must have at least 1 member");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (next_hop_list.list == NULL)
    {
        SWSS_LOG_ERROR("next hop list is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    for (uint32_t i = 0; i < next_hop_list.count; ++i)
    {
        // check if all next hops on list exist and are created

        sai_object_id_t next_hop_id = next_hop_list.list[i];

        if (local_next_hops_set.find(next_hop_id) == local_next_hops_set.end())
        {
            SWSS_LOG_ERROR("next hop %llx not found", next_hop_id);

            return SAI_STATUS_INVALID_PARAMETER;
        }

        // TODO check for next hop duplication on list
    }

    sai_status_t status = redis_generic_create(
            SAI_OBJECT_TYPE_NEXT_HOP_GROUP,
            next_hop_group_id,
            attr_count,
            attr_list);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("inserting next hop group %llx to local state", *next_hop_group_id);

        local_next_hop_groups_set.insert(*next_hop_group_id);

        // TODO increase reference count for used object ids
    }

    return status;
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

    // TODO check if it safe to remove next hop group,
    // whether is used by any route or by any acl entry

    if (local_next_hop_groups_set.find(next_hop_group_id) == local_next_hop_groups_set.end())
    {
        SWSS_LOG_ERROR("next hop group %llx is missing", next_hop_group_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_remove(
            SAI_OBJECT_TYPE_NEXT_HOP_GROUP,
            next_hop_group_id);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("erasing next hop group %llx from local state", next_hop_group_id);

        local_next_hops_set.erase(next_hop_group_id);
    }

    return status;
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

    if (attr == NULL)
    {
        SWSS_LOG_ERROR("attribute parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (local_next_hop_groups_set.find(next_hop_group_id) == local_next_hop_groups_set.end())
    {
        SWSS_LOG_ERROR("next hop group %llx is missing", next_hop_group_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    switch (attr->id)
    {
        // currently next hop group don't have attributes that can be set
        default:

            SWSS_LOG_ERROR("setting attribute id %d is not supported", attr->id);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_set(
            SAI_OBJECT_TYPE_NEXT_HOP_GROUP,
            next_hop_group_id,
            attr);

    return status;
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

    if (local_next_hop_groups_set.find(next_hop_group_id) == local_next_hop_groups_set.end())
    {
        SWSS_LOG_ERROR("next hop group %llx is missing", next_hop_group_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        sai_attribute_t* attr = &attr_list[i];

        switch (attr->id)
        {
            case SAI_NEXT_HOP_GROUP_ATTR_NEXT_HOP_COUNT:
            case SAI_NEXT_HOP_GROUP_ATTR_TYPE:
            case SAI_NEXT_HOP_GROUP_ATTR_NEXT_HOP_LIST:
                // ok
                break;

            default:

                SWSS_LOG_ERROR("getting attribute id %d is not supported", attr->id);

                return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    sai_status_t status = redis_generic_get(
            SAI_OBJECT_TYPE_NEXT_HOP_GROUP,
            next_hop_group_id,
            attr_count,
            attr_list);

    return status;
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
