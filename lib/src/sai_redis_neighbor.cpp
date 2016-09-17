#include "sai_redis.h"

std::set<std::string> local_neighbor_entries_set;

sai_status_t redis_validate_neighbor_entry(
    _In_ const sai_neighbor_entry_t* neighbor_entry)
{
    SWSS_LOG_ENTER();

    if (neighbor_entry == NULL)
    {
        SWSS_LOG_ERROR("neighbor_entry is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (neighbor_entry->rif_id == 0)
    {
        SWSS_LOG_ERROR("neighbor_entry.rif_id is zero");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_object_id_t rif_id = neighbor_entry->rif_id;

    sai_object_type_t rif_type = sai_object_type_query(rif_id);

    if (rif_type != SAI_OBJECT_TYPE_ROUTER_INTERFACE)
    {
        SWSS_LOG_ERROR("neighbor_entry.rif_id type is not SAI_OBJECT_TYPE_ROUTER_INTERFACE: %d, id: %llx", rif_type, rif_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // TODO check if ip address is correct (as spearate api)

    if (local_router_interfaces_set.find(rif_id) == local_router_interfaces_set.end())
    {
        SWSS_LOG_ERROR("router interface id %llx is does not exists", rif_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    return SAI_STATUS_SUCCESS;
}

bool redis_validate_contains_attribute(
    _In_ sai_attr_id_t required_id,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    if (attr_list == NULL)
    {
        SWSS_LOG_ERROR("attribute list is null");

        return false;
    }

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        if (attr_list[i].id == required_id)
        {
            // attribute is on list
            return true;
        }
    }

    return false;
}

#define VALIDATE_NEIGHBOR_ENTRY(x) { \
    sai_status_t r = redis_validate_neighbor_entry(x); \
    if (r != SAI_STATUS_SUCCESS) { return r; } }

/**
 * Routine Description:
 *    @brief Create neighbor entry
 *
 * Arguments:
 *    @param[in] neighbor_entry - neighbor entry
 *    @param[in] attr_count - number of attributes
 *    @param[in] attrs - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 *
 * Note: IP address expected in Network Byte Order.
 */
sai_status_t redis_create_neighbor_entry(
    _In_ const sai_neighbor_entry_t* neighbor_entry,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    VALIDATE_NEIGHBOR_ENTRY(neighbor_entry);

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

    if (redis_validate_contains_attribute(
                SAI_NEIGHBOR_ATTR_DST_MAC_ADDRESS,
                attr_count,
                attr_list) == false)
    {
        SWSS_LOG_ERROR("missing attribute SAI_NEIGHBOR_ATTR_DST_MAC_ADDRESS");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    std::string str_neighbor_entry;
    sai_serialize_neighbor_entry(*neighbor_entry, str_neighbor_entry);

    if (local_neighbor_entries_set.find(str_neighbor_entry) != local_neighbor_entries_set.end())
    {
        SWSS_LOG_ERROR("neighbor_entry %s already exists", str_neighbor_entry.c_str());

        return SAI_STATUS_ITEM_ALREADY_EXISTS;
    }

    sai_status_t status = redis_generic_create(
            SAI_OBJECT_TYPE_NEIGHBOR,
            neighbor_entry,
            attr_count,
            attr_list);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("inserting neighbor entry %s to local state", str_neighbor_entry.c_str());

        local_neighbor_entries_set.insert(str_neighbor_entry);

        // TODO increase router interface reference count to prevent delete
    }

    return status;
}

/**
 * Routine Description:
 *    @brief Remove neighbor entry
 *
 * Arguments:
 *    @param[in] neighbor_entry - neighbor entry
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 *
 * Note: IP address expected in Network Byte Order.
 */
sai_status_t redis_remove_neighbor_entry(
    _In_ const sai_neighbor_entry_t* neighbor_entry)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    VALIDATE_NEIGHBOR_ENTRY(neighbor_entry);

    std::string str_neighbor_entry;
    sai_serialize_neighbor_entry(*neighbor_entry, str_neighbor_entry);

    if (local_neighbor_entries_set.find(str_neighbor_entry) == local_neighbor_entries_set.end())
    {
        SWSS_LOG_ERROR("neighbor_entry %s is missing", str_neighbor_entry.c_str());

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // TODO check dependencies if it is safe to remove

    sai_status_t status = redis_generic_remove(
            SAI_OBJECT_TYPE_NEIGHBOR,
            neighbor_entry);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("erasing neighbor entry %s from local state", str_neighbor_entry.c_str());

        local_neighbor_entries_set.erase(str_neighbor_entry);

        // TODO decrease router interface count
    }

    return status;
}

/**
 * Routine Description:
 *    @brief Set neighbor attribute value
 *
 * Arguments:
 *    @param[in] neighbor_entry - neighbor entry
 *    @param[in] attr - attribute
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_set_neighbor_attribute(
    _In_ const sai_neighbor_entry_t* neighbor_entry,
    _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    VALIDATE_NEIGHBOR_ENTRY(neighbor_entry);

    if (attr == NULL)
    {
        SWSS_LOG_ERROR("attribute parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // TODO this attribute id validation should be automatic
    // and generic when we will have SAI headers metadata
    switch (attr->id)
    {
        case SAI_NEIGHBOR_ATTR_DST_MAC_ADDRESS:
        case SAI_NEIGHBOR_ATTR_PACKET_ACTION:
        case SAI_NEIGHBOR_ATTR_NO_HOST_ROUTE:
        case SAI_NEIGHBOR_ATTR_META_DATA:
            // ok;
            break;

        default:

            SWSS_LOG_ERROR("set attribute id %d is not allowed", attr->id);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    std::string str_neighbor_entry;
    sai_serialize_neighbor_entry(*neighbor_entry, str_neighbor_entry);

    if (local_neighbor_entries_set.find(str_neighbor_entry) == local_neighbor_entries_set.end())
    {
        SWSS_LOG_ERROR("neighbor_entry %s is missing", str_neighbor_entry.c_str());

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_set(
            SAI_OBJECT_TYPE_NEIGHBOR,
            neighbor_entry,
            attr);

    return status;
}

/**
 * Routine Description:
 *    @brief Get neighbor attribute value
 *
 * Arguments:
 *    @param[in] neighbor_entry - neighbor entry
 *    @param[in] attr_count - number of attributes
 *    @param[inout] attrs - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_get_neighbor_attribute(
    _In_ const sai_neighbor_entry_t* neighbor_entry,
    _In_ uint32_t attr_count,
    _Inout_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    VALIDATE_NEIGHBOR_ENTRY(neighbor_entry);

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

    std::string str_neighbor_entry;
    sai_serialize_neighbor_entry(*neighbor_entry, str_neighbor_entry);

    if (local_neighbor_entries_set.find(str_neighbor_entry) == local_neighbor_entries_set.end())
    {
        SWSS_LOG_ERROR("neighbor_entry %s is missing", str_neighbor_entry.c_str());

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_get(
            SAI_OBJECT_TYPE_NEIGHBOR,
            neighbor_entry,
            attr_count,
            attr_list);

    return status;
}

/**
 * Routine Description:
 *    @brief Remove all neighbor entries
 *
 * Arguments:
 *    None
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_remove_all_neighbor_entries(void)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

/**
 * @brief neighbor table methods, retrieved via sai_api_query()
 */
const sai_neighbor_api_t redis_neighbor_api = {
    redis_create_neighbor_entry,
    redis_remove_neighbor_entry,
    redis_set_neighbor_attribute,
    redis_get_neighbor_attribute,
    redis_remove_all_neighbor_entries,
};

