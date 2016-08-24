#include "sai_redis.h"
#include <cstring>

std::set<sai_object_id_t> local_hostif_trap_groups_set;
std::set<sai_object_id_t> local_hostifs_set;

// TODO this will be changed to object is as well
std::set<sai_hostif_trap_id_t> local_hostif_traps_set;
std::set<sai_hostif_user_defined_trap_id_t> local_user_defined_hostif_traps_set;

/**
 * Routine Description:
 *   @brief hostif receive function
 *
 * Arguments:
 *    @param[in] hif_id - host interface id
 *    @param[out] buffer - packet buffer
 *    @param[in,out] buffer_size - @param[in] allocated buffer size. @param[out] actual packet size in bytes
 *    @param[in,out] attr_count - @param[in] allocated list size. @param[out] number of attributes
 *    @param[out] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            SAI_STATUS_BUFFER_OVERFLOW if buffer_size is insufficient,
 *            and buffer_size will be filled with required size. Or
 *            if attr_count is insufficient, and attr_count
 *            will be filled with required count.
 *            Failure status code on error
 */
sai_status_t redis_recv_packet(
        _In_ sai_object_id_t hif_id,
        _Out_ void *buffer,
        _Inout_ sai_size_t *buffer_size,
        _Inout_ uint32_t *attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

/**
 * Routine Description:
 *   @brief hostif send function
 *
 * Arguments:
 *    @param[in] hif_id - host interface id. only valid for send through FD channel. Use SAI_NULL_OBJECT_ID for send through CB channel.
 *    @param[in] buffer - packet buffer
 *    @param[in] buffer size - packet size in bytes
 *    @param[in] attr_count - number of attributes
 *    @param[in] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_send_packet(
        _In_ sai_object_id_t hif_id,
        _In_ void *buffer,
        _In_ sai_size_t buffer_size,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

/**
* Routine Description:
*   @brief Set user defined trap attribute value.
*
* Arguments:
*    @param[in] hostif_user_defined_trap_id - host interface user defined trap id
*    @param[in] attr - attribute
*
* Return Values:
*    @return SAI_STATUS_SUCCESS on success
*            Failure status code on error
*/
sai_status_t redis_set_user_defined_trap_attribute(
    _In_ sai_hostif_user_defined_trap_id_t hostif_user_defined_trapid,
    _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr == NULL)
    {
        SWSS_LOG_ERROR("attribute parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    //TODO to make this work we need to populate user trap list first

    if (local_user_defined_hostif_traps_set.find(hostif_user_defined_trapid) == local_user_defined_hostif_traps_set.end())
    {
        SWSS_LOG_ERROR("host interface user defined trap %llx is missing", hostif_user_defined_trapid);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    switch (attr->id)
    {
        case SAI_HOSTIF_USER_DEFINED_TRAP_ATTR_TRAP_CHANNEL:

            {
                sai_hostif_trap_channel_t trap_channel = (sai_hostif_trap_channel_t)attr->value.s32;

                switch (trap_channel)
                {
                    case SAI_HOSTIF_TRAP_CHANNEL_FD:
                    case SAI_HOSTIF_TRAP_CHANNEL_CB:
                    case SAI_HOSTIF_TRAP_CHANNEL_NETDEV:
                        // ok
                        break;

                    default:

                        SWSS_LOG_ERROR("invalid trap channel value: %d", trap_channel);

                        return SAI_STATUS_INVALID_PARAMETER;
                }
            }

            // TODO extra validation here maybe needed to check cb/fd/netdev
            // we will need extra logic to validate this attribute (previous attributes)
            break;

        case SAI_HOSTIF_USER_DEFINED_TRAP_ATTR_FD:

            {
                // Valid only when SAI_HOSTIF_TRAP_ATTR_TRAP_CHANNEL == SAI_HOSTIF_TRAP_CHANNEL_FD
                // Must be set before set SAI_HOSTIF_TRAP_ATTR_TRAP_CHANNEL to SAI_HOSTIF_TRAP_CHANNEL_FD
                //
                // when we move to create api, this can be solved other way

                sai_object_id_t fd = attr->value.oid;

                if (local_hostifs_set.find(fd) == local_hostifs_set.end() &&
                    fd != SAI_NULL_OBJECT_ID)
                {
                    SWSS_LOG_ERROR("file descriptor %llx is missing", fd);

                    return SAI_STATUS_INVALID_PARAMETER;
                }

                // TODO additional logic here is required, this fd hostif
                // must be type of sai_hostif_type_t == SAI_HOSTIF_TYPE_FD
                // we need to check hostif attribute then
                //
                // TODO also what about netdev case ?
            }

            break;

        default:

            SWSS_LOG_ERROR("setting attribute id %d is not supported", attr->id);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_set(
            SAI_OBJECT_TYPE_TRAP_USER_DEF,
            hostif_user_defined_trapid,
            attr);

    return status;
}

/**
* Routine Description:
*   @brief Get user defined trap attribute value.
*
* Arguments:
*    @param[in] hostif_user_defined_trap_id - host interface user defined trap id
*    @param[in] attr_count - number of attributes
*    @param[in,out] attr_list - array of attributes
*
* Return Values:
*    @return SAI_STATUS_SUCCESS on success
*            Failure status code on error
*/
sai_status_t redis_get_user_defined_trap_attribute(
    _In_ sai_hostif_user_defined_trap_id_t hostif_user_defined_trapid,
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

    if (local_user_defined_hostif_traps_set.find(hostif_user_defined_trapid) == local_user_defined_hostif_traps_set.end())
    {
        SWSS_LOG_ERROR("host interface user defined trap %llx is missing", hostif_user_defined_trapid);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        sai_attribute_t* attr = &attr_list[i];

        switch (attr->id)
        {
            case SAI_HOSTIF_USER_DEFINED_TRAP_ATTR_TRAP_CHANNEL:
            case SAI_HOSTIF_USER_DEFINED_TRAP_ATTR_FD:
                break;

            default:

                SWSS_LOG_ERROR("getting attribute id %d is not supported", attr->id);

                return SAI_STATUS_INVALID_PARAMETER;
        }
    }


    sai_status_t status = redis_generic_get(
            SAI_OBJECT_TYPE_TRAP_USER_DEF,
            hostif_user_defined_trapid,
            attr_count,
            attr_list);

    return status;
}

/**
 * Routine Description:
 *    @brief Create host interface trap group
 *
 * Arguments:
 *   @param[out] hostif_trap_group_id - host interface trap group id
 *   @param[in] attr_count - number of attributes
 *   @param[in] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_create_hostif_trap_group(
    _Out_ sai_object_id_t *hostif_trap_group_id,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr_count > 0 && attr_list == NULL)
    {
        SWSS_LOG_ERROR("attribute list is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    const sai_attribute_t* attr_policer = redis_get_attribute_by_id(SAI_HOSTIF_TRAP_GROUP_ATTR_POLICER, attr_count, attr_list);

    sai_object_id_t policer_id = SAI_NULL_OBJECT_ID; // default, NULL object is valid (need flag in metadata)

    if (attr_policer != NULL)
    {
        policer_id = attr_policer->value.oid;

        if (policer_id != SAI_NULL_OBJECT_ID)
        {
            if (local_policers_set.find(policer_id) == local_policers_set.end())
            {
                SWSS_LOG_ERROR("policer %llx is missing", policer_id);

                return SAI_STATUS_INVALID_PARAMETER;
            }
        }
    }

    sai_status_t status = redis_generic_create(
            SAI_OBJECT_TYPE_TRAP_GROUP,
            hostif_trap_group_id,
            attr_count,
            attr_list);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("inserting hostif trap group %llx to local state", *hostif_trap_group_id);

        local_hostif_trap_groups_set.insert(*hostif_trap_group_id);

        // TODO increase reference count for used object ids
    }

    return status;
}

/**
 * Routine Description:
 *    @brief Remove host interface trap group
 *
 * Arguments:
 *   @param[in] hostif_trap_group_id - host interface trap group id
 *
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_remove_hostif_trap_group(
    _In_ sai_object_id_t hostif_trap_group_id)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    // TODO check if it safe to remove trap group member

    if (local_hostif_trap_groups_set.find(hostif_trap_group_id) == local_hostif_trap_groups_set.end())
    {
        SWSS_LOG_ERROR("hostif trap group %llx is missing", hostif_trap_group_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_remove(
            SAI_OBJECT_TYPE_TRAP_GROUP,
            hostif_trap_group_id);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("erasing hostif trap group %llx from local state", hostif_trap_group_id);

        local_hostif_trap_groups_set.erase(hostif_trap_group_id);
    }

    return status;
}

/**
 * Routine Description:
 *   @brief Set host interface trap group attribute value.
 *
 * Arguments:
 *    @param[in] hostif_trap_group_id - host interface trap group id
 *    @param[in] attr - attribute
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_set_trap_group_attribute(
    _In_ sai_object_id_t hostif_trap_group_id,
    _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr == NULL)
    {
        SWSS_LOG_ERROR("attribute parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (local_hostif_trap_groups_set.find(hostif_trap_group_id) == local_hostif_trap_groups_set.end())
    {
        SWSS_LOG_ERROR("hostif trap group %llx is missing", hostif_trap_group_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    switch (attr->id)
    {
        case SAI_HOSTIF_TRAP_GROUP_ATTR_ADMIN_STATE:
            break;

        case SAI_HOSTIF_TRAP_GROUP_ATTR_QUEUE:

            // TODO this can be tricky since this QUEUE is queue INDEX
            // indirect depenency, by default there are 8 queues, but
            // user can create extra one, so there may be 10, and what
            // happens when this points to queue 10 and user remove this queue?
            // on queue remove we should queue index on trap group and
            // not allow to remove then
            break;

        case SAI_HOSTIF_TRAP_GROUP_ATTR_POLICER:

            {
                sai_object_id_t policer_id = SAI_NULL_OBJECT_ID; // default, NULL object is valid (need flag in metadata)

                policer_id = attr->value.oid;

                if (policer_id != SAI_NULL_OBJECT_ID)
                {
                    if (local_policers_set.find(policer_id) == local_policers_set.end())
                    {
                        SWSS_LOG_ERROR("policer %llx is missing", policer_id);

                        return SAI_STATUS_INVALID_PARAMETER;
                    }
                }
            }

            break;

        default:

            SWSS_LOG_ERROR("setting attribute id %d is not supported", attr->id);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_set(
            SAI_OBJECT_TYPE_TRAP_GROUP,
            hostif_trap_group_id,
            attr);

    return status;
}

/**
 * Routine Description:
 *   @brief get host interface trap group attribute value.
 *
 * Arguments:
 *    @param[in] hostif_trap_group_id - host interface trap group id
 *    @param[in] attr_count - number of attributes
 *    @param[in,out] attr_list - array of attributes
 *
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_get_trap_group_attribute(
    _In_ sai_object_id_t hostif_trap_group_id,
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

    if (local_hostif_trap_groups_set.find(hostif_trap_group_id) == local_hostif_trap_groups_set.end())
    {
        SWSS_LOG_ERROR("hostif trap group %llx is missing", hostif_trap_group_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        sai_attribute_t* attr = &attr_list[i];

        switch (attr->id)
        {
            case SAI_HOSTIF_TRAP_GROUP_ATTR_ADMIN_STATE:
            case SAI_HOSTIF_TRAP_GROUP_ATTR_QUEUE:
            case SAI_HOSTIF_TRAP_GROUP_ATTR_POLICER:
                // ok
                break;

            default:

                SWSS_LOG_ERROR("getting attribute id %d is not supported", attr->id);

                return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    sai_status_t status = redis_generic_get(
            SAI_OBJECT_TYPE_TRAP_GROUP,
            hostif_trap_group_id,
            attr_count,
            attr_list);

    return status;
}

/**
 * Routine Description:
 *   @brief Set trap attribute value.
 *
 * Arguments:
 *    @param[in] hostif_trap_id - host interface trap id
 *    @param[in] attr - attribute
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_set_trap_attribute(
    _In_ sai_hostif_trap_id_t hostif_trapid,
    _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr == NULL)
    {
        SWSS_LOG_ERROR("attribute parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // TODO to make this work we need to populate trap list first

    if (local_hostif_traps_set.find(hostif_trapid) == local_hostif_traps_set.end())
    {
        SWSS_LOG_ERROR("host interface trap %d is missing", hostif_trapid);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    switch (attr->id)
    {
        case SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION:

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

        case SAI_HOSTIF_TRAP_ATTR_TRAP_PRIORITY:
            break;

        case SAI_HOSTIF_TRAP_ATTR_TRAP_CHANNEL:

            {
                sai_hostif_trap_channel_t trap_channel = (sai_hostif_trap_channel_t)attr->value.s32;

                switch (trap_channel)
                {
                    case SAI_HOSTIF_TRAP_CHANNEL_FD:
                    case SAI_HOSTIF_TRAP_CHANNEL_CB:
                    case SAI_HOSTIF_TRAP_CHANNEL_NETDEV:
                        // ok
                        break;

                    default:

                        SWSS_LOG_ERROR("invalid trap channel value: %d", trap_channel);

                        return SAI_STATUS_INVALID_PARAMETER;
                }
            }

            // TODO extra validation here maybe needed to check cb/fd/netdev
            // we will need extra logic to validate this attribute (previous attributes)
            break;

        case SAI_HOSTIF_TRAP_ATTR_FD:

            {
                // Valid only when SAI_HOSTIF_TRAP_ATTR_TRAP_CHANNEL == SAI_HOSTIF_TRAP_CHANNEL_FD
                // Must be set before set SAI_HOSTIF_TRAP_ATTR_TRAP_CHANNEL to SAI_HOSTIF_TRAP_CHANNEL_FD
                //
                // when we move to create api, this can be solved other way

                sai_object_id_t fd = attr->value.oid;

                if (local_hostifs_set.find(fd) == local_hostifs_set.end() &&
                    fd != SAI_NULL_OBJECT_ID)
                {
                    SWSS_LOG_ERROR("file descriptor %llx is missing", fd);

                    return SAI_STATUS_INVALID_PARAMETER;
                }

                // TODO additional logic here is required, this fd hostif
                // must be type of sai_hostif_type_t == SAI_HOSTIF_TYPE_FD
                // we need to check hostif attribute then
                //
                // TODO also what about netdev case ?
            }

            break;

        // TODO condition to check when ports are removed
        case SAI_HOSTIF_TRAP_ATTR_PORT_LIST:

            {
                sai_object_list_t port_list = attr->value.objlist;

                // TODO should we allow null on SET when count is zero ?
                if (port_list.list == NULL)
                {
                    SWSS_LOG_ERROR("port list is NULL");

                    return SAI_STATUS_INVALID_PARAMETER;
                }

                for (uint32_t i = 0; i < port_list.count; ++i)
                {
                    sai_object_id_t port_id = port_list.list[i];

                    if (local_ports_set.find(port_id) == local_ports_set.end())
                    {
                        SWSS_LOG_ERROR("port %llx is missing", port_id);

                        return SAI_STATUS_INVALID_PARAMETER;
                    }
                }
            }

            break;

        case SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP:

            {
                sai_object_id_t trap_group = attr->value.oid;

                if (local_default_trap_group_id != SAI_NULL_OBJECT_ID &&
                    local_default_trap_group_id == trap_group)
                {
                    // ok its default trap id
                    break;
                }

                // TODO that default trap group should also be
                // on that trap groups list this would implify condition
                if (local_hostif_trap_groups_set.find(trap_group) == local_hostif_trap_groups_set.end())
                {
                    SWSS_LOG_ERROR("trap group %llx is missing", trap_group);

                    return SAI_STATUS_INVALID_PARAMETER;
                }
            }

            break;

        default:

            SWSS_LOG_ERROR("setting attribute id %d is not supported", attr->id);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_set(
            SAI_OBJECT_TYPE_TRAP,
            hostif_trapid,
            attr);

    return status;
}

/**
 * Routine Description:
 *   @brief Get trap attribute value.
 *
 * Arguments:
 *    @param[in] hostif_trap_id - host interface trap id
 *    @param[in] attr_count - number of attributes
 *    @param[in,out] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_get_trap_attribute(
    _In_ sai_hostif_trap_id_t hostif_trapid,
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

    if (local_hostif_traps_set.find(hostif_trapid) == local_hostif_traps_set.end())
    {
        SWSS_LOG_ERROR("host interface trap %d is missing", hostif_trapid);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        sai_attribute_t* attr = &attr_list[i];

        switch (attr->id)
        {
            case SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION:
            case SAI_HOSTIF_TRAP_ATTR_TRAP_PRIORITY:
            case SAI_HOSTIF_TRAP_ATTR_TRAP_CHANNEL:
            case SAI_HOSTIF_TRAP_ATTR_FD:
                break;

            case SAI_HOSTIF_TRAP_ATTR_PORT_LIST:

            {
                sai_object_list_t port_list = attr->value.objlist;

                // TODO should we allow null on GET when count is zero ?
                // will it be used then to obtain actual count ?
                if (port_list.list == NULL)
                {
                    SWSS_LOG_ERROR("port list is NULL");

                    return SAI_STATUS_INVALID_PARAMETER;
                }
            }

            default:

                SWSS_LOG_ERROR("getting attribute id %d is not supported", attr->id);

                return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    sai_status_t status = redis_generic_get(
            SAI_OBJECT_TYPE_TRAP,
            hostif_trapid,
            attr_count,
            attr_list);

    return status;
}

/**
 * Routine Description:
 *    @brief Create host interface
 *
 * Arguments:
 *    @param[out] hif_id - host interface id
 *    @param[in] attr_count - number of attributes
 *    @param[in] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_create_hostif(
    _Out_ sai_object_id_t * hif_id,
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

    if (attr_count < 1)
    {
        SWSS_LOG_ERROR("attribute count must be at least 2");

        // SAI_HOSTIF_ATTR_TYPE
        //
        // - conditional
        // SAI_HOSTIF_ATTR_RIF_OR_PORT_ID
        // SAI_HOSTIF_ATTR_NAME

        return SAI_STATUS_INVALID_PARAMETER;
    }

    const sai_attribute_t* attr_type = redis_get_attribute_by_id(SAI_HOSTIF_ATTR_TYPE, attr_count, attr_list);

    const sai_attribute_t* attr_rif_or_port_id = redis_get_attribute_by_id(SAI_HOSTIF_ATTR_RIF_OR_PORT_ID, attr_count, attr_list);
    const sai_attribute_t* attr_name = redis_get_attribute_by_id(SAI_HOSTIF_ATTR_NAME, attr_count, attr_list);

    if (attr_type == NULL)
    {
        SWSS_LOG_ERROR("missing type attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    sai_hostif_type_t type = (sai_hostif_type_t)attr_type->value.s32;

    switch (type)
    {
        case SAI_HOSTIF_TYPE_NETDEV:

            if (attr_rif_or_port_id == NULL)
            {
                SWSS_LOG_ERROR("missing rif or port id attribute");

                return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
            }

            // STIF_NAME_SIZE
            // TODO validation logic - RIF or PORT, can be ether one
            {
                sai_object_id_t rif_or_port_id = attr_rif_or_port_id->value.oid;

                if (local_ports_set.find(rif_or_port_id) == local_ports_set.end() &&
                    local_router_interfaces_set.find(rif_or_port_id) == local_router_interfaces_set.end())
                {
                    // TODO we could get actual type of that object
                    SWSS_LOG_ERROR("rif or port %llx is missing", rif_or_port_id);

                    return SAI_STATUS_INVALID_PARAMETER;
                }
            }

            break;

        case SAI_HOSTIF_TYPE_FD:

            if (attr_name == NULL)
            {
                SWSS_LOG_ERROR("missing name attribute");

                return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
            }

            {
                // TODO we could validate name here HOSTIF_NAME_SIZE - 1 \0 this will be extra logic
                const char* chardata = attr_name->value.chardata;

                size_t len = strnlen(chardata, HOSTIF_NAME_SIZE);

                if (len == HOSTIF_NAME_SIZE)
                {
                    SWSS_LOG_ERROR("host interface name is too long");

                    return SAI_STATUS_INVALID_PARAMETER;
                }

                if (len == 0)
                {
                    SWSS_LOG_ERROR("host interface name is zero");

                    return SAI_STATUS_INVALID_PARAMETER;
                }

                for (size_t i = 0; i < len; ++i)
                {
                    char c = chardata[i];

                    if (c < 0x20 || c > 0x7e)
                    {
                        SWSS_LOG_ERROR("interface name contains invalid character 0x%02x", c);

                        return SAI_STATUS_INVALID_PARAMETER;
                    }
                }

                // TODO check if it contains only ASCII and whether name is not used
                // by other host interface
            }

            break;


        default:

            SWSS_LOG_ERROR("invalid type attribute value: %d", type);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_create(
            SAI_OBJECT_TYPE_HOST_INTERFACE,
            hif_id,
            attr_count,
            attr_list);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("host interface %llx to local state", *hif_id);

        local_hostifs_set.insert(*hif_id);

        // TODO increase reference count for used object ids
    }

    return status;
}

/**
 * Routine Description:
 *    @brief Remove host interface
 *
 * Arguments:
 *    @param[in] hif_id - host interface id
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_remove_hostif(
    _In_ sai_object_id_t hif_id)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    // TODO check hostif can be safely removed

    if (local_hostifs_set.find(hif_id) == local_hostifs_set.end())
    {
        SWSS_LOG_ERROR("host interface %llx is missing", hif_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_remove(
            SAI_OBJECT_TYPE_HOST_INTERFACE,
            hif_id);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("erasing host interface %llx from local state", hif_id);

        local_hostifs_set.erase(hif_id);
    }

    return status;
}

/**
 * Routine Description:
 *    @brief Set host interface attribute
 *
 * Arguments:
 *    @param[in] hif_id - host interface id
 *    @param[in] attr - attribute
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_set_hostif_attribute(
    _In_ sai_object_id_t hif_id,
    _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr == NULL)
    {
        SWSS_LOG_ERROR("attribute parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (local_hostifs_set.find(hif_id) == local_hostifs_set.end())
    {
        SWSS_LOG_ERROR("host interface %llx is missing", hif_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    switch (attr->id)
    {
        case SAI_HOSTIF_ATTR_OPER_STATUS:
            // ok
            break;

        default:

            SWSS_LOG_ERROR("setting attribute id %d is not supported", attr->id);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_set(
            SAI_OBJECT_TYPE_HOST_INTERFACE,
            hif_id,
            attr);

    return status;
}

/**
 * Routine Description:
 *    @brief Get host interface attribute
 *
 * Arguments:
 *    @param[in] hif_id - host interface id
 *    @param[in] attr_count - number of attributes
 *    @param[inout] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_get_hostif_attribute(
    _In_ sai_object_id_t hif_id,
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

    if (local_hostifs_set.find(hif_id) == local_hostifs_set.end())
    {
        SWSS_LOG_ERROR("host interface %llx is missing", hif_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        sai_attribute_t* attr = &attr_list[i];

        switch (attr->id)
        {
            case SAI_HOSTIF_ATTR_TYPE:
            case SAI_HOSTIF_ATTR_RIF_OR_PORT_ID:
            case SAI_HOSTIF_ATTR_NAME:
            case SAI_HOSTIF_ATTR_OPER_STATUS:
                // ok
                break;

            default:

                SWSS_LOG_ERROR("getting attribute id %d is not supported", attr->id);

                return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    sai_status_t status = redis_generic_get(
            SAI_OBJECT_TYPE_HOST_INTERFACE,
            hif_id,
            attr_count,
            attr_list);

    return status;
}

/**
 * @brief hostif methods table retrieved with sai_api_query()
 */
const sai_hostif_api_t redis_host_interface_api = {
    redis_create_hostif,
    redis_remove_hostif,
    redis_set_hostif_attribute,
    redis_get_hostif_attribute,

    redis_create_hostif_trap_group,
    redis_remove_hostif_trap_group,
    redis_set_trap_group_attribute,
    redis_get_trap_group_attribute,

    redis_set_trap_attribute,
    redis_get_trap_attribute,

    redis_set_user_defined_trap_attribute,
    redis_get_user_defined_trap_attribute,

    redis_recv_packet,
    redis_send_packet,
};
