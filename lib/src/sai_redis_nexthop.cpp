#include "sai_redis.h"

std::set<sai_object_id_t> local_next_hops_set;

/**
 * Routine Description:
 *    @brief Create next hop
 *
 * Arguments:
 *    @param[out] next_hop_id - next hop id
 *    @param[in] attr_count - number of attributes
 *    @param[in] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 *
 * Note: IP address expected in Network Byte Order.
 */
sai_status_t redis_create_next_hop(
    _Out_ sai_object_id_t* next_hop_id,
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

        // SAI_NEXT_HOP_ATTR_TYPE
        // SAI_NEXT_HOP_ATTR_ROUTER_INTERFACE_ID
        //
        // - contitional on create depending on type
        // SAI_NEXT_HOP_ATTR_IP
        // SAI_NEXT_HOP_ATTR_TUNNEL_ID

        return SAI_STATUS_INVALID_PARAMETER;
    }

    const sai_attribute_t* attr_type = redis_get_attribute_by_id(SAI_NEXT_HOP_ATTR_TYPE, attr_count, attr_list);
    const sai_attribute_t* attr_rif_id = redis_get_attribute_by_id(SAI_NEXT_HOP_ATTR_ROUTER_INTERFACE_ID, attr_count, attr_list);

    const sai_attribute_t* attr_ip = redis_get_attribute_by_id(SAI_NEXT_HOP_ATTR_IP, attr_count, attr_list);
    const sai_attribute_t* attr_tunnel_id = redis_get_attribute_by_id(SAI_NEXT_HOP_ATTR_TUNNEL_ID, attr_count, attr_list);

    if (attr_type == NULL)
    {
        SWSS_LOG_ERROR("missing type attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    sai_next_hop_type_t type = (sai_next_hop_type_t)attr_type->value.s32;

    switch (type)
    {
        case SAI_NEXT_HOP_IP:

            if (attr_ip == NULL)
            {
                SWSS_LOG_ERROR("ip attribute is missing");

                return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
            }

            // TODO validate IP address, can it be 0.0.0.0 ?

            break;

        case SAI_NEXT_HOP_TUNNEL_ENCAP:

            if (attr_tunnel_id == NULL)
            {
                SWSS_LOG_ERROR("tunnel id attribute is missing");

                return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
            }

            {
                // TODO those checks should be automated when metadata will be valid
                sai_object_id_t tunnel_id = attr_tunnel_id->value.oid;

                if (local_tunnels_set.find(tunnel_id) == local_tunnels_set.end())
                {
                    SWSS_LOG_ERROR("tunnel %llx is missing", tunnel_id);

                    return SAI_STATUS_INVALID_PARAMETER;
                }

                // TODO tunnel is existing, should be make some additional checks ? like tunnel encap type?
            }

            break;

        default:

            SWSS_LOG_ERROR("invalid type attribute value: %d", type);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_rif_id == NULL)
    {
        SWSS_LOG_ERROR("missing router interface id attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    sai_object_id_t rif_id = attr_rif_id->value.oid;

    if (local_router_interfaces_set.find(rif_id) == local_router_interfaces_set.end())
    {
        SWSS_LOG_ERROR("router interface %llx is missing", rif_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_create(
            SAI_OBJECT_TYPE_NEXT_HOP,
            next_hop_id,
            attr_count,
            attr_list);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("inserting next hop %llx to local state", *next_hop_id);

        local_next_hops_set.insert(*next_hop_id);

        // TODO increase reference count for used object ids
    }

    return status;
}

/**
 * Routine Description:
 *    @brief Remove next hop
 *
 * Arguments:
 *    @param[in] next_hop_id - next hop id
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_remove_next_hop(
    _In_ sai_object_id_t next_hop_id)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    // TODO check if it safe to remove next hop, whether used by next hop group or route
    // mainly we want to check reference count on that object

    if (local_next_hops_set.find(next_hop_id) == local_next_hops_set.end())
    {
        SWSS_LOG_ERROR("next hop %llx is missing", next_hop_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_remove(
            SAI_OBJECT_TYPE_NEXT_HOP,
            next_hop_id);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("erasing next hop %llx from local state", next_hop_id);

        local_next_hops_set.erase(next_hop_id);
    }

    return status;
}

/**
 * Routine Description:
 *    @brief Set Next Hop attribute
 *
 * Arguments:
 *    @param[in] next_hop_id - next hop id
 *    @param[in] attr - attribute
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_set_next_hop_attribute(
    _In_ sai_object_id_t next_hop_id,
    _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr == NULL)
    {
        SWSS_LOG_ERROR("attribute parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (local_next_hops_set.find(next_hop_id) == local_next_hops_set.end())
    {
        SWSS_LOG_ERROR("next hop %llx is missing", next_hop_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    switch (attr->id)
    {
        // currently next hop don't have attributes that can be set
        default:

            SWSS_LOG_ERROR("setting attribute id %d is not supported", attr->id);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_set(
            SAI_OBJECT_TYPE_NEXT_HOP,
            next_hop_id,
            attr);

    return status;
}

/**
 * Routine Description:
 *    @brief Get Next Hop attribute
 *
 * Arguments:
 *    @param[in] next_hop_id - next hop id
 *    @param[in] attr_count - number of attributes
 *    @param[inout] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_get_next_hop_attribute(
    _In_ sai_object_id_t next_hop_id,
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

    if (local_next_hops_set.find(next_hop_id) == local_next_hops_set.end())
    {
        SWSS_LOG_ERROR("next hop %llx is missing", next_hop_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        sai_attribute_t* attr = &attr_list[i];

        switch (attr->id)
        {
            case SAI_NEXT_HOP_ATTR_TYPE:
            case SAI_NEXT_HOP_ATTR_IP:
            case SAI_NEXT_HOP_ATTR_ROUTER_INTERFACE_ID:
            case SAI_NEXT_HOP_ATTR_TUNNEL_ID:
                // ok
                break;

            default:

                SWSS_LOG_ERROR("getting attribute id %d is not supported", attr->id);

                return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    sai_status_t status = redis_generic_get(
            SAI_OBJECT_TYPE_NEXT_HOP,
            next_hop_id,
            attr_count,
            attr_list);

    return status;
}

/**
 * @brief Next Hop methods table retrieved with sai_api_query()
 */
const sai_next_hop_api_t redis_next_hop_api = {
    redis_create_next_hop,
    redis_remove_next_hop,
    redis_set_next_hop_attribute,
    redis_get_next_hop_attribute,
};
