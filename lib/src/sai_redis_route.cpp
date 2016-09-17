#include "sai_redis.h"

std::set<std::string> local_route_entries_set;

sai_status_t redis_validate_route_entry(
    _In_ const sai_unicast_route_entry_t* unicast_route_entry)
{
    SWSS_LOG_ENTER();

    if (unicast_route_entry == NULL)
    {
        SWSS_LOG_ERROR("route_entry is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (unicast_route_entry->vr_id == 0)
    {
        SWSS_LOG_ERROR("route_entry.vr_id is zero");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_object_id_t vr_id = unicast_route_entry->vr_id;

    sai_object_type_t vr_type = sai_object_type_query(vr_id);

    if (vr_type != SAI_OBJECT_TYPE_VIRTUAL_ROUTER)
    {
        SWSS_LOG_ERROR("route_entry.vr_id type is not SAI_OBJECT_VIRTUAL_ROUTER: %d, id: %llx", vr_type, vr_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // TODO check if ip address is correct (as spearate api)

    // TODO make this as function like isValidVirtualRouterId()
    if ((local_virtual_routers_set.find(vr_id) == local_virtual_routers_set.end()) &&
        (local_default_virtual_router_id != vr_id))
    {
        SWSS_LOG_ERROR("virtual router %llx is missing", vr_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    return SAI_STATUS_SUCCESS;
}

#define VALIDATE_ROUTE_ENTRY(x) { \
    sai_status_t r = redis_validate_route_entry(x); \
    if (r != SAI_STATUS_SUCCESS) { return r; } }

/**
 * Routine Description:
 *    @brief Create Route
 *
 * Arguments:
 *    @param[in] unicast_route_entry - route entry
 *    @param[in] attr_count - number of attributes
 *    @param[in] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 *
 * Note: IP prefix/mask expected in Network Byte Order.
 *
 */
sai_status_t redis_create_route(
    _In_ const sai_unicast_route_entry_t* unicast_route_entry,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    VALIDATE_ROUTE_ENTRY(unicast_route_entry);

    if (attr_list == NULL)
    {
        SWSS_LOG_ERROR("attribute list parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    const sai_attribute_t* attr_next_hop = redis_get_attribute_by_id(SAI_ROUTE_ATTR_NEXT_HOP_ID, attr_count, attr_list);

    if (attr_next_hop != NULL)
    {
        sai_object_id_t next_hop = attr_next_hop->value.oid;

        sai_object_type_t type = sai_object_type_query(next_hop);

        // TODO increment reference count
        switch (type)
        {
            case SAI_OBJECT_TYPE_NEXT_HOP:

                if (local_next_hops_set.find(next_hop) == local_next_hops_set.end())
                {
                    SWSS_LOG_ERROR("next hop %llx is missing", next_hop);

                    return SAI_STATUS_INVALID_PARAMETER;
                }

                break;

            case SAI_OBJECT_TYPE_NEXT_HOP_GROUP:

                if (local_next_hop_groups_set.find(next_hop) == local_next_hop_groups_set.end())
                {
                    SWSS_LOG_ERROR("next hop group %llx is missing", next_hop);

                    return SAI_STATUS_INVALID_PARAMETER;
                }

                break;

            case SAI_OBJECT_TYPE_ROUTER_INTERFACE:

                if (local_router_interfaces_set.find(next_hop) == local_router_interfaces_set.end())
                {
                    SWSS_LOG_ERROR("router interface %llx is missing", next_hop);

                    return SAI_STATUS_INVALID_PARAMETER;
                }

                break;

            // TODO it may be also a CPU port in some case

            default:

                SWSS_LOG_ERROR("next hop object have invalid object type: %d, id: %llx", type, next_hop);

                return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    std::string str_route_entry;
    sai_serialize_route_entry(*unicast_route_entry, str_route_entry);

    if (local_route_entries_set.find(str_route_entry) != local_route_entries_set.end())
    {
        SWSS_LOG_ERROR("route_entry %s already exists", str_route_entry.c_str());

        return SAI_STATUS_ITEM_ALREADY_EXISTS;
    }

    sai_status_t status = redis_generic_create(
            SAI_OBJECT_TYPE_ROUTE,
            unicast_route_entry,
            attr_count,
            attr_list);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("inserting route entry %s to local state", str_route_entry.c_str());

        local_route_entries_set.insert(str_route_entry);

        // TODO increase virtual router reference count to prevent delete and possibly other object count
    }

    return status;
}

/**
 * Routine Description:
 *    @brief Remove Route
 *
 * Arguments:
 *    @param[in] unicast_route_entry - route entry
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 *
 * Note: IP prefix/mask expected in Network Byte Order.
 */
sai_status_t redis_remove_route(
    _In_ const sai_unicast_route_entry_t* unicast_route_entry)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    VALIDATE_ROUTE_ENTRY(unicast_route_entry);

    std::string str_route_entry;
    sai_serialize_route_entry(*unicast_route_entry, str_route_entry);

    if (local_route_entries_set.find(str_route_entry) == local_route_entries_set.end())
    {
        SWSS_LOG_ERROR("route_entry %s is missing", str_route_entry.c_str());

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // since route is a leaf, it is always safe to remove route

    sai_status_t status = redis_generic_remove(
            SAI_OBJECT_TYPE_ROUTE,
            unicast_route_entry);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("erasing route entry %s from local state", str_route_entry.c_str());

        local_route_entries_set.erase(str_route_entry);

        // TODO decrease virtual router count and possibly other object count (next hop etc)
    }

    return status;
}

/**
 * Routine Description:
 *    @brief Set route attribute value
 *
 * Arguments:
 *    @param[in] unicast_route_entry - route entry
 *    @param[in] attr - attribute
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_set_route_attribute(
    _In_ const sai_unicast_route_entry_t* unicast_route_entry,
    _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    VALIDATE_ROUTE_ENTRY(unicast_route_entry);

    if (attr == NULL)
    {
        SWSS_LOG_ERROR("attribute parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // we can set only writtable attributes
    switch (attr->id)
    {
        case SAI_ROUTE_ATTR_TRAP_PRIORITY:
        case SAI_ROUTE_ATTR_META_DATA:
            // ok
            break;

        default:

            SWSS_LOG_ERROR("setting attribute id %d is not supported", attr->id);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    std::string str_route_entry;
    sai_serialize_route_entry(*unicast_route_entry, str_route_entry);

    if (local_route_entries_set.find(str_route_entry) == local_route_entries_set.end())
    {
        SWSS_LOG_ERROR("route_entry %s is missing", str_route_entry.c_str());

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_set(
            SAI_OBJECT_TYPE_ROUTE,
            unicast_route_entry,
            attr);

    return status;
}

/**
 * Routine Description:
 *    @brief Get route attribute value
 *
 * Arguments:
 *    @param[in] unicast_route_entry - route entry
 *    @param[in] attr_count - number of attributes
 *    @param[inout] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_get_route_attribute(
    _In_ const sai_unicast_route_entry_t* unicast_route_entry,
    _In_ uint32_t attr_count,
    _Inout_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    VALIDATE_ROUTE_ENTRY(unicast_route_entry);

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

    std::string str_route_entry;
    sai_serialize_route_entry(*unicast_route_entry, str_route_entry);

    if (local_route_entries_set.find(str_route_entry) == local_route_entries_set.end())
    {
        SWSS_LOG_ERROR("route_entry %s is missing", str_route_entry.c_str());

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_get(
            SAI_OBJECT_TYPE_ROUTE,
            unicast_route_entry,
            attr_count,
            attr_list);

    return status;
}

/**
 * @brief Router entry methods table retrieved with sai_api_query()
 */
const sai_route_api_t redis_route_api = {
    redis_create_route,
    redis_remove_route,
    redis_set_route_attribute,
    redis_get_route_attribute,
};
