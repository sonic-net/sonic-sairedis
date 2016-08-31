#include "sai_redis.h"

std::set<sai_vlan_id_t> local_vlans_set;
std::set<sai_object_id_t> local_vlan_members_set;

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

    if (local_vlans_set.find(vlan_id) != local_vlans_set.end())
    {
        SWSS_LOG_ERROR("vlan %d already exists", vlan_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (vlan_id < MINIMUM_VLAN_NUMBER || vlan_id > MAXIMUM_VLAN_NUMBER)
    {
        SWSS_LOG_ERROR("invalid vlan number %d", vlan_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_create_vlan(
            SAI_OBJECT_TYPE_VLAN,
            vlan_id);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("inserting vlan %d to local state", vlan_id);

        local_vlans_set.insert(vlan_id);
    }

    return status;
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

    // TODO can vlan 1 be removed at all ? or it must exist without members ?

    if (vlan_id == DEFAULT_VLAN_NUMBER)
    {
        SWSS_LOG_ERROR("default vlan %d can't be removed", vlan_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (local_vlans_set.find(vlan_id) == local_vlans_set.end())
    {
        SWSS_LOG_ERROR("vlan %d is missing", vlan_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // TODO check if it is safe to remove vlan:
    // need to check: vlan members, FDB, router_interface, port?
    // SAI_PORT_ATTR_PORT_VLAN_ID on port can be set

    sai_status_t status = redis_generic_remove_vlan(
            SAI_OBJECT_TYPE_VLAN,
            vlan_id);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("erasing vlan %d from local state", vlan_id);

        local_vlans_set.erase(vlan_id);
    }

    return status;
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

    if (attr == NULL)
    {
        SWSS_LOG_ERROR("attribute parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (local_vlans_set.find(vlan_id) == local_vlans_set.end())
    {
        SWSS_LOG_ERROR("vlan %d is missing", vlan_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    switch (attr->id)
    {
        case SAI_VLAN_ATTR_MAX_LEARNED_ADDRESSES:
        case SAI_VLAN_ATTR_STP_INSTANCE:
        case SAI_VLAN_ATTR_LEARN_DISABLE:
        case SAI_VLAN_ATTR_META_DATA:
            // ok
            break;

        default:

            SWSS_LOG_ERROR("setting attribute id %d is not supported", attr->id);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_set_vlan(
            SAI_OBJECT_TYPE_VLAN,
            vlan_id,
            attr);

    return status;
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

    if (local_vlans_set.find(vlan_id) == local_vlans_set.end())
    {
        SWSS_LOG_ERROR("vlan %d is missing", vlan_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        sai_attribute_t* attr = &attr_list[i];

        switch (attr->id)
        {
            case SAI_VLAN_ATTR_MEMBER_LIST:

                {
                    sai_object_list_t vlan_member_list = attr->value.objlist;

                    if (vlan_member_list.list == NULL)
                    {
                        SWSS_LOG_ERROR("vlan member list is NULL");

                        return SAI_STATUS_INVALID_PARAMETER;
                    }
                }

                break;

            case SAI_VLAN_ATTR_MAX_LEARNED_ADDRESSES:
            case SAI_VLAN_ATTR_STP_INSTANCE:
            case SAI_VLAN_ATTR_LEARN_DISABLE:
            case SAI_VLAN_ATTR_META_DATA:
                // ok
                break;

            default:

                SWSS_LOG_ERROR("getting attribute id %d is not supported", attr->id);

                return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    sai_status_t status = redis_generic_get_vlan(
            SAI_OBJECT_TYPE_VLAN,
            vlan_id,
            attr_count,
            attr_list);

    // TODO this a hack to get member list for vlan 1, this should be done at
    // init and there may be some issues when we will start keeping attributes
    // of vlan members in local state

    if (status == SAI_STATUS_SUCCESS && vlan_id == DEFAULT_VLAN_NUMBER)
    {
        const sai_attribute_t* attr_member_list = redis_get_attribute_by_id(SAI_VLAN_ATTR_MEMBER_LIST, attr_count, attr_list);

        if (attr_member_list != NULL)
        {
            sai_object_list_t member_list = attr_member_list->value.objlist;

            for (uint32_t i = 0; i < member_list.count; ++i)
            {
                sai_object_id_t member_id = member_list.list[i];

                if (local_vlan_members_set.find(member_id) == local_vlan_members_set.end())
                {
                    SWSS_LOG_NOTICE("adding vlan member %llx to vlan %d", member_id, DEFAULT_VLAN_NUMBER);

                    local_vlan_members_set.insert(member_id);
                }
            }
        }
    }

    return status;
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

    if (attr_list == NULL)
    {
        SWSS_LOG_ERROR("attribute list parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_count < 2)
    {
        SWSS_LOG_ERROR("attribute count must be at least 2");

        // SAI_VLAN_MEMBER_ATTR_VLAN_ID
        // SAI_VLAN_MEMBER_ATTR_PORT_ID

        return SAI_STATUS_INVALID_PARAMETER;
    }

    const sai_attribute_t* attr_vlan_id = redis_get_attribute_by_id(SAI_VLAN_MEMBER_ATTR_VLAN_ID, attr_count, attr_list);
    const sai_attribute_t* attr_port_id = redis_get_attribute_by_id(SAI_VLAN_MEMBER_ATTR_PORT_ID, attr_count, attr_list);

    if (attr_vlan_id == NULL)
    {
        SWSS_LOG_ERROR("missing vlan id attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    // TODO check if it should be u16
    sai_vlan_id_t vlan_id = attr_vlan_id->value.u16;

    if (local_vlans_set.find(vlan_id) == local_vlans_set.end())
    {
        SWSS_LOG_ERROR("vlan %d is missing", vlan_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_port_id == NULL)
    {
        SWSS_LOG_ERROR("missing port id attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    sai_object_id_t port_id = attr_port_id->value.oid;

    sai_object_type_t port_type = sai_object_type_query(port_id);

    if (port_id != SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_ERROR("port id is zero");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (port_type != SAI_OBJECT_TYPE_PORT)
    {
        SWSS_LOG_ERROR("port id object type is not SAI_OBJECT_TYPE_PORT: %d, id: %llx", port_type, port_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // TODO check port id, it may be logical port, check if this port exists on list

    // TODO if vlanmember can be a LAG and a PORT and the same port
    // can be lag member is that a conflict ? or is it allowed?

    sai_status_t status = redis_generic_create(
            SAI_OBJECT_TYPE_VLAN_MEMBER,
            vlan_member_id,
            attr_count,
            attr_list);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("inserting vlan member %llx to local state", *vlan_member_id);

        local_vlan_members_set.insert(*vlan_member_id);

        // TODO increase reference count for used object ids
    }
    return status;
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

    // TODO check if vlan member can be removed
    // vlan member is leaf so it should be always possible to remove it

    if (local_vlan_members_set.find(vlan_member_id) == local_vlan_members_set.end())
    {
        SWSS_LOG_ERROR("vlan member %llx is missing", vlan_member_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // TODO check dependencies if vlan member can be safely removed

    sai_status_t status = redis_generic_remove(
            SAI_OBJECT_TYPE_VLAN_MEMBER,
            vlan_member_id);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("erasing vlan member %llx from local state", vlan_member_id);

        local_vlan_members_set.erase(vlan_member_id);
    }

    return status;
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

    if (attr == NULL)
    {
        SWSS_LOG_ERROR("attribute parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (local_vlan_members_set.find(vlan_member_id) == local_vlan_members_set.end())
    {
        SWSS_LOG_ERROR("vlan member %llx is missing", vlan_member_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    switch (attr->id)
    {
        case SAI_VLAN_MEMBER_ATTR_TAGGING_MODE:
            // ok
            break;

        default:

            SWSS_LOG_ERROR("setting attribute id %d is not supported", attr->id);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_set(
            SAI_OBJECT_TYPE_VLAN_MEMBER,
            vlan_member_id,
            attr);

    return status;
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

    if (local_vlan_members_set.find(vlan_member_id) == local_vlan_members_set.end())
    {
        SWSS_LOG_ERROR("vlan member %llx is missing", vlan_member_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        sai_attribute_t* attr = &attr_list[i];

        switch (attr->id)
        {
            case SAI_VLAN_MEMBER_ATTR_VLAN_ID:
            case SAI_VLAN_MEMBER_ATTR_PORT_ID:
            case SAI_VLAN_MEMBER_ATTR_TAGGING_MODE:
                // ok
                break;

            default:

                SWSS_LOG_ERROR("getting attribute id %d is not supported", attr->id);

                return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    sai_status_t status = redis_generic_get(
            SAI_OBJECT_TYPE_VLAN_MEMBER,
            vlan_member_id,
            attr_count,
            attr_list);

    return status;
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
