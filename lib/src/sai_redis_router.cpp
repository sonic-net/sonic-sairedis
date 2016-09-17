#include "sai_redis.h"

sai_object_id_t local_default_virtual_router_id = SAI_NULL_OBJECT_ID;

std::set<sai_object_id_t> local_virtual_routers_set;

/**
 * Routine Description:
 *    @brief Create virtual router
 *
 * Arguments:
 *    @param[out] vr_id - virtual router id
 *    @param[in] attr_count - number of attributes
 *    @param[in] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            SAI_STATUS_ADDR_NOT_FOUND if neither SAI_SWITCH_ATTR_SRC_MAC_ADDRESS nor
 *            SAI_VIRTUAL_ROUTER_ATTR_SRC_MAC_ADDRESS is set.
 */
sai_status_t redis_create_virtual_router(
    _Out_ sai_object_id_t *vr_id,
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

    sai_status_t status = redis_generic_create(
            SAI_OBJECT_TYPE_VIRTUAL_ROUTER,
            vr_id,
            attr_count,
            attr_list);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("inserting virtual router %llx to local state", *vr_id);

        local_virtual_routers_set.insert(*vr_id);
    }

    return status;
}

/**
 * Routine Description:
 *    @brief Remove virtual router
 *
 * Arguments:
 *    @param[in] vr_id - virtual router id
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_remove_virtual_router(
    _In_ sai_object_id_t vr_id)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (vr_id == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_ERROR("virtual router id is zero");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (local_virtual_routers_set.find(vr_id) == local_virtual_routers_set.end())
    {
        if (vr_id == local_default_virtual_router_id)
        {
            SWSS_LOG_ERROR("default virtual router with id %llx cannot be removed", vr_id);

            return SAI_STATUS_INVALID_PARAMETER;
        }

        SWSS_LOG_ERROR("virtual router %llx is missing", vr_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // TODO check this vr_id dependencies for router interface and route
    // before calling actual remove (object reference)

    sai_status_t status = redis_generic_remove(
            SAI_OBJECT_TYPE_VIRTUAL_ROUTER,
            vr_id);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("erasing virtual router %llx from local state", vr_id);

        local_virtual_routers_set.erase(vr_id);
    }

    return status;
}

/**
 * Routine Description:
 *    @brief Set virtual router attribute Value
 *
 * Arguments:
 *    @param[in] vr_id - virtual router id
 *    @param[in] attr - attribute
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_set_virtual_router_attribute(
    _In_ sai_object_id_t vr_id,
    _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr == NULL)
    {
        SWSS_LOG_ERROR("attribute parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (vr_id == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_ERROR("virtual router id is zero");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if ((local_virtual_routers_set.find(vr_id) == local_router_interfaces_set.end()) &&
        (vr_id != local_default_virtual_router_id))
    {
        SWSS_LOG_ERROR("virtual router %llx is missing", vr_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    switch (attr->id)
    {
        case SAI_VIRTUAL_ROUTER_ATTR_ADMIN_V4_STATE:
        case SAI_VIRTUAL_ROUTER_ATTR_ADMIN_V6_STATE:
        case SAI_VIRTUAL_ROUTER_ATTR_SRC_MAC_ADDRESS:
        case SAI_VIRTUAL_ROUTER_ATTR_VIOLATION_TTL1_ACTION:
        case SAI_VIRTUAL_ROUTER_ATTR_VIOLATION_IP_OPTIONS:
            // ok
            break;

        default:

            SWSS_LOG_ERROR("set attribute id %d is not allowed", attr->id);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_set(
            SAI_OBJECT_TYPE_VIRTUAL_ROUTER,
            vr_id,
            attr);

    return status;
}

/**
 * Routine Description:
 *    @brief Get virtual router attribute Value
 *
 * Arguments:
 *    @param[in] vr_id - virtual router id
 *    @param[in] attr_count - number of attributes
 *    @param[in] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_get_virtual_router_attribute(
    _In_ sai_object_id_t vr_id,
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

    if (vr_id == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_ERROR("virtual router id is zero");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if ((local_virtual_routers_set.find(vr_id) == local_router_interfaces_set.end()) &&
        (vr_id != local_default_virtual_router_id))
    {
        SWSS_LOG_ERROR("virtual router %llx is missing", vr_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_get(
            SAI_OBJECT_TYPE_VIRTUAL_ROUTER,
            vr_id,
            attr_count,
            attr_list);

    return status;
}

/**
 * @brief Virtual router methods table retrieved with sai_api_query()
 */
const sai_virtual_router_api_t redis_router_api = {
    redis_create_virtual_router,
    redis_remove_virtual_router,
    redis_set_virtual_router_attribute,
    redis_get_virtual_router_attribute,
};
