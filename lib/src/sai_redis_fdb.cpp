#include "sai_redis.h"

std::set<std::string> local_fdb_entries_set;

sai_status_t redis_validate_fdb_entry(
    _In_ const sai_fdb_entry_t* fdb_entry)
{
    SWSS_LOG_ENTER();

    if (fdb_entry == NULL)
    {
        SWSS_LOG_ERROR("route_entry is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // TODO can mac be 00:00:00:00:00:00 ?

    sai_vlan_id_t vlan_id = fdb_entry->vlan_id;

    if (local_vlans_set.find(vlan_id) == local_vlans_set.end())
    {
        SWSS_LOG_ERROR("vlan %u is missing", vlan_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    return SAI_STATUS_SUCCESS;
}

#define VALIDATE_FDB_ENTRY(x) { \
    sai_status_t r = redis_validate_fdb_entry(x); \
    if (r != SAI_STATUS_SUCCESS) { return r; } }

/**
 * Routine Description:
 *    @brief Create FDB entry
 *
 * Arguments:
 *    @param[in] fdb_entry - fdb entry
 *    @param[in] attr_count - number of attributes
 *    @param[in] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_create_fdb_entry(
    _In_ const sai_fdb_entry_t *fdb_entry,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    VALIDATE_FDB_ENTRY(fdb_entry);

    std::string str_fdb_entry;
    sai_serialize_primitive(*fdb_entry, str_fdb_entry);

    if (local_fdb_entries_set.find(str_fdb_entry) != local_fdb_entries_set.end())
    {
        SWSS_LOG_ERROR("fdb entry %s already exists", str_fdb_entry.c_str());

        return SAI_STATUS_ITEM_ALREADY_EXISTS;
    }

    if (attr_count < 3)
    {
        SWSS_LOG_ERROR("attribute count must be at least 3");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_list == NULL)
    {
        SWSS_LOG_ERROR("attribute list parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    const sai_attribute_t* attr_type = redis_get_attribute_by_id(SAI_FDB_ENTRY_ATTR_TYPE, attr_count, attr_list);
    const sai_attribute_t* attr_port_id = redis_get_attribute_by_id(SAI_FDB_ENTRY_ATTR_PORT_ID, attr_count, attr_list);
    const sai_attribute_t* attr_packet_action = redis_get_attribute_by_id(SAI_FDB_ENTRY_ATTR_PACKET_ACTION, attr_count, attr_list);

    if (attr_type == NULL)
    {
        SWSS_LOG_ERROR("missing type attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    sai_fdb_entry_type_t type = (sai_fdb_entry_type_t)attr_type->value.s32;

    switch (type)
    {
        case SAI_FDB_ENTRY_DYNAMIC:
        case SAI_FDB_ENTRY_STATIC:
            // ok
            break;

        default:

            SWSS_LOG_ERROR("invalid type attribute value: %d", type);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_port_id == NULL)
    {
        SWSS_LOG_ERROR("missing port id attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    sai_object_id_t port_id = attr_port_id->value.oid;

    if (port_id == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_ERROR("port id is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (local_ports_set.find(port_id) == local_ports_set.end() &&
        local_lags_set.find(port_id) == local_lags_set.end() &&
        local_tunnels_set.find(port_id) == local_tunnels_set.end())    // TODO tunnel may require some extra validation (l2 entry)
    {
        SWSS_LOG_ERROR("missing port id %llx", port_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_packet_action == NULL)
    {
        SWSS_LOG_ERROR("missing packet action attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    sai_packet_action_t packet_action = (sai_packet_action_t)attr_packet_action->value.s32;

    switch (packet_action)
    {
        case SAI_PACKET_ACTION_DROP:
        case SAI_PACKET_ACTION_FORWARD:
        case SAI_PACKET_ACTION_COPY:
        case SAI_PACKET_ACTION_COPY_CANCEL:
        case SAI_PACKET_ACTION_TRAP:
        case SAI_PACKET_ACTION_LOG:
        case SAI_PACKET_ACTION_DENY:
        case SAI_PACKET_ACTION_TRANSIT:
            // ok
            break;

        default:

            SWSS_LOG_ERROR("invalid packet action value: %d", packet_action);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    const sai_attribute_t* attr_meta_data = redis_get_attribute_by_id(SAI_FDB_ENTRY_ATTR_META_DATA, attr_count, attr_list);

    if (attr_meta_data != NULL)
    {
        // TODO validation range must be checked via SAI_SWITCH_ATTR_FDB_DST_USER_META_DATA_RANGE / saiswitch

        uint32_t meta_data = attr_meta_data->value.u32;

        SWSS_LOG_DEBUG("fdb metadata value: %u", meta_data);
    }

    sai_status_t status = redis_generic_create(
            SAI_OBJECT_TYPE_FDB,
            fdb_entry,
            attr_count,
            attr_list);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("inserting fdb entry %s to local state", str_fdb_entry.c_str());

        local_fdb_entries_set.insert(str_fdb_entry);

        // TODO increase port id reference
        // TODO should vlan reference be increased ?
    }

    return status;
}

/**
 * Routine Description:
 *    @brief Remove FDB entry
 *
 * Arguments:
 *    @param[in] fdb_entry - fdb entry
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_remove_fdb_entry(
    _In_ const sai_fdb_entry_t* fdb_entry)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    VALIDATE_FDB_ENTRY(fdb_entry);

    std::string str_fdb_entry;
    sai_serialize_primitive(*fdb_entry, str_fdb_entry);

    if (local_fdb_entries_set.find(str_fdb_entry) == local_fdb_entries_set.end())
    {
        SWSS_LOG_ERROR("fdb_entry %s is missing", str_fdb_entry.c_str());

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // since fdb is a leaf, it is always safe to remove fdb

    sai_status_t status = redis_generic_remove(
            SAI_OBJECT_TYPE_FDB,
            fdb_entry);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("erasing fdb entry %s from local state", str_fdb_entry.c_str());

        local_fdb_entries_set.erase(str_fdb_entry);

        // TODO decrease references
    }
    return status;
}

/**
 * Routine Description:
 *    @brief Set fdb entry attribute value
 *
 * Arguments:
 *    @param[in] fdb_entry - fdb entry
 *    @param[in] attr - attribute
 * * Return Values: *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_set_fdb_entry_attribute(
    _In_ const sai_fdb_entry_t* fdb_entry,
    _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    VALIDATE_FDB_ENTRY(fdb_entry);

    std::string str_fdb_entry;
    sai_serialize_primitive(*fdb_entry, str_fdb_entry);

    if (local_fdb_entries_set.find(str_fdb_entry) == local_fdb_entries_set.end())
    {
        SWSS_LOG_ERROR("fdb entry %s is missing", str_fdb_entry.c_str());

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr == NULL)
    {
        SWSS_LOG_ERROR("attribute parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // TODO validation logic can be the same as in create
    // we can set only writtable attributes
    switch (attr->id)
    {
        case SAI_FDB_ENTRY_ATTR_TYPE:

            {
                sai_fdb_entry_type_t type = (sai_fdb_entry_type_t)attr->value.s32;

                switch (type)
                {
                    case SAI_FDB_ENTRY_DYNAMIC:
                    case SAI_FDB_ENTRY_STATIC:
                        // ok
                        break;

                    default:

                        SWSS_LOG_ERROR("invalid type attribute value: %d", type);

                        return SAI_STATUS_INVALID_PARAMETER;
                }
            }

            break;

        case SAI_FDB_ENTRY_ATTR_PORT_ID:

            {
                sai_object_id_t port_id = attr->value.oid;

                if (port_id == SAI_NULL_OBJECT_ID)
                {
                    SWSS_LOG_ERROR("port id is NULL");

                    return SAI_STATUS_INVALID_PARAMETER;
                }

                if (local_ports_set.find(port_id) == local_ports_set.end() &&
                    local_lags_set.find(port_id) == local_lags_set.end() &&
                    local_tunnels_set.find(port_id) == local_tunnels_set.end())    // TODO tunnel may require some extra validation (l2 entry)
                {
                    SWSS_LOG_ERROR("missing port id %llx", port_id);

                    return SAI_STATUS_INVALID_PARAMETER;
                }
            }

            break;

        case SAI_FDB_ENTRY_ATTR_PACKET_ACTION:

            {
                sai_packet_action_t packet_action = (sai_packet_action_t)attr->value.s32;

                switch (packet_action)
                {
                    case SAI_PACKET_ACTION_DROP:
                    case SAI_PACKET_ACTION_FORWARD:
                    case SAI_PACKET_ACTION_COPY:
                    case SAI_PACKET_ACTION_COPY_CANCEL:
                    case SAI_PACKET_ACTION_TRAP:
                    case SAI_PACKET_ACTION_LOG:
                    case SAI_PACKET_ACTION_DENY:
                    case SAI_PACKET_ACTION_TRANSIT:
                        // ok
                        break;

                    default:

                        SWSS_LOG_ERROR("invalid packet action value: %d", packet_action);

                        return SAI_STATUS_INVALID_PARAMETER;
                }
            }

            break;

        case SAI_FDB_ENTRY_ATTR_META_DATA:

            // TODO validate metadata
            // depends on SAI_SWITCH_ATTR_FDB_DST_USER_META_DATA_RANGE
            break;

        default:

            SWSS_LOG_ERROR("setting attribute id %d is not supported", attr->id);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_set(
            SAI_OBJECT_TYPE_FDB,
            fdb_entry,
            attr);

    return status;
}

/**
 * Routine Description:
 *    @brief Get fdb entry attribute value
 *
 * Arguments:
 *    @param[in] fdb_entry - fdb entry
 *    @param[in] attr_count - number of attributes
 *    @param[inout] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_get_fdb_entry_attribute(
    _In_ const sai_fdb_entry_t* fdb_entry,
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

    std::string str_fdb_entry;
    sai_serialize_primitive(*fdb_entry, str_fdb_entry);

    if (local_fdb_entries_set.find(str_fdb_entry) == local_fdb_entries_set.end())
    {
        SWSS_LOG_ERROR("fdb entry %s is missing", str_fdb_entry.c_str());

        return SAI_STATUS_INVALID_PARAMETER;
    }
    
    for (uint32_t i = 0; i < attr_count; ++i)
    {
        sai_attribute_t* attr = &attr_list[i];

        switch (attr->id)
        {
            case SAI_FDB_ENTRY_ATTR_TYPE:
            case SAI_FDB_ENTRY_ATTR_PORT_ID:
            case SAI_FDB_ENTRY_ATTR_PACKET_ACTION:
            case SAI_FDB_ENTRY_ATTR_META_DATA:
                // ok
                break;

            default:

                SWSS_LOG_ERROR("getting attribute id %d is not supported", attr->id);

                return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    sai_status_t status = redis_generic_get(
            SAI_OBJECT_TYPE_FDB,
            fdb_entry,
            attr_count,
            attr_list);

    return status;
}

/**
 * Routine Description:
 *    @brief Remove all FDB entries by attribute set in sai_fdb_flush_attr
 *
 * Arguments:
 *    @param[in] attr_count - number of attributes
 *    @param[in] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_flush_fdb_entries(
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

/**
 * @brief FDB method table retrieved with sai_api_query()
 */
const sai_fdb_api_t redis_fdb_api = {
    redis_create_fdb_entry,
    redis_remove_fdb_entry,
    redis_set_fdb_entry_attribute,
    redis_get_fdb_entry_attribute,
    redis_flush_fdb_entries,
};
