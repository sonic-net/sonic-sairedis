#include "sai_redis.h"

std::set<sai_object_id_t> local_router_interfaces_set;

const sai_attribute_t* redis_get_attribute_by_id(
    _In_ sai_attr_id_t id,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    if (attr_list == NULL)
    {
        SWSS_LOG_ERROR("attribute list is null");

        return NULL;
    }

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        if (attr_list[i].id == id)
        {
            // attribute is on list
            return &attr_list[i];
        }
    }

    return NULL;
}

/**
 * Routine Description:
 *    @brief Create router interface.
 *
 * Arguments:
 *    @param[out] rif_id - router interface id
 *    @param[in] attr_count - number of attributes
 *    @param[in] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_create_router_interface(
    _Out_ sai_object_id_t* rif_id,
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

        // SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID and SAI_ROUTER_INTERFACE_ATTR_TYPE
        // SAI_ROUTER_INTERFACE_ATTR_PORT_ID when SAI_ROUTER_INTERFACE_ATTR_TYPE == SAI_ROUTER_INTERFACE_TYPE_PORT
        // SAI_ROUTER_INTERFACE_ATTR_VLAN_ID when SAI_ROUTER_INTERFACE_ATTR_TYPE == SAI_ROUTER_INTERFACE_TYPE_VLAN

        return SAI_STATUS_INVALID_PARAMETER;
    }

    const sai_attribute_t* attr_vr_id = redis_get_attribute_by_id(SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID, attr_count, attr_list);
    const sai_attribute_t* attr_type = redis_get_attribute_by_id(SAI_ROUTER_INTERFACE_ATTR_TYPE, attr_count, attr_list);

    const sai_attribute_t* attr_port_id = redis_get_attribute_by_id(SAI_ROUTER_INTERFACE_ATTR_PORT_ID, attr_count, attr_list);
    const sai_attribute_t* attr_vlan_id = redis_get_attribute_by_id(SAI_ROUTER_INTERFACE_ATTR_VLAN_ID, attr_count, attr_list);

    if (attr_vr_id == NULL)
    {
        SWSS_LOG_ERROR("missing virtual router id attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    sai_object_id_t vr_id = attr_vr_id->value.oid;

    if (vr_id == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_ERROR("virtual router id is zero");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_object_type_t type = sai_object_type_query(vr_id);

    if (type != SAI_OBJECT_TYPE_VIRTUAL_ROUTER)
    {
        SWSS_LOG_ERROR("virtual router id type is not SAI_OBJECT_TYPE_VIRTUAL_ROUTER: %d, id: %llx", type, vr_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // TODO make this as function like isValidVirtualRouter()
    if ((local_virtual_routers_set.find(vr_id) == local_virtual_routers_set.end()) &&
        (local_default_virtual_router_id != vr_id))
    {
        SWSS_LOG_ERROR("virtual router %llx is missing", vr_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_type == NULL)
    {
        SWSS_LOG_ERROR("missing type attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    int32_t ri_type = attr_type->value.s32;

    switch (ri_type)
    {
        case SAI_ROUTER_INTERFACE_TYPE_PORT:

            {
                if (attr_port_id == NULL)
                {
                    SWSS_LOG_ERROR("missing port attribute");

                    return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
                }

                sai_object_id_t port_id = attr_port_id->value.oid;

                type = sai_object_type_query(port_id);

                switch (type)
                {
                    case SAI_OBJECT_TYPE_PORT:

                        if (local_ports_set.find(port_id) == local_ports_set.end())
                        {
                            SWSS_LOG_ERROR("port %llx is missing", port_id);

                            return SAI_STATUS_INVALID_PARAMETER;
                        }

                        break;

                    case SAI_OBJECT_TYPE_LAG:

                        if (local_lags_set.find(port_id) == local_lags_set.end())
                        {
                            SWSS_LOG_ERROR("lag %llx is mising", port_id);

                            return SAI_STATUS_INVALID_PARAMETER;
                        }

                        break;

                    default:

                        SWSS_LOG_ERROR("port id type %d is not LAG or PORT, id: %llx", type, port_id);

                        return SAI_STATUS_INVALID_PARAMETER;
                }
            }

            break;

        case SAI_ROUTER_INTERFACE_TYPE_VLAN:

            if (attr_vlan_id == NULL)
            {
                SWSS_LOG_ERROR("missing vlan id attribute");

                return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
            }

            // TODO indirect dependency, since vlan is a number, this must be
            // checked if router interface exists before removing vlan
            {
                sai_vlan_id_t vlan_id = attr_vlan_id->value.u16;

                if (local_vlans_set.find(vlan_id) == local_vlans_set.end())
                {
                    SWSS_LOG_ERROR("vlan %d is missing", vlan_id);

                    return SAI_STATUS_INVALID_PARAMETER;
                }
            }

            break;

        case SAI_ROUTER_INTERFACE_TYPE_LOOPBACK:
            // ok
            break;

        default:

            SWSS_LOG_ERROR("type attribute have invalid value: %d", ri_type);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_create(
            SAI_OBJECT_TYPE_ROUTER_INTERFACE,
            rif_id,
            attr_count,
            attr_list);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("inserting router interface %llx to local state", *rif_id);

        local_router_interfaces_set.insert(*rif_id);
    }

    return status;
}

/**
 * Routine Description:
 *    @brief Remove router interface
 *
 * Arguments:
 *    @param[in] rif_id - router interface id
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_remove_router_interface(
    _In_ sai_object_id_t rif_id)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (local_router_interfaces_set.find(rif_id) == local_router_interfaces_set.end())
    {
        SWSS_LOG_ERROR("router interface %llx is missing", rif_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // TODO check this rif_id reference count before remove!
    // route, next hop, next hop group, neighbor entry should modify reference count

    sai_status_t status = redis_generic_remove(
            SAI_OBJECT_TYPE_ROUTER_INTERFACE,
            rif_id);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("erasing router interface %llx from local state", rif_id);

        local_router_interfaces_set.erase(rif_id);
    }

    return status;
}

/**
 * Routine Description:
 *    @brief Set router interface attribute
 *
 * Arguments:
 *    @param[in] rif_id - router interface id
 *    @param[in] attr - attribute
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_set_router_interface_attribute(
    _In_ sai_object_id_t rif_id,
    _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr == NULL)
    {
        SWSS_LOG_ERROR("attribute parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (local_router_interfaces_set.find(rif_id) == local_router_interfaces_set.end())
    {
        SWSS_LOG_ERROR("router interface %llx is missing", rif_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr->id == SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID ||
        attr->id == SAI_ROUTER_INTERFACE_ATTR_TYPE ||
        attr->id == SAI_ROUTER_INTERFACE_ATTR_PORT_ID ||
        attr->id == SAI_ROUTER_INTERFACE_ATTR_VLAN_ID)
    {
        SWSS_LOG_ERROR("attribute is marked as CREATE_ONLY: %d", attr->id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_set(
            SAI_OBJECT_TYPE_ROUTER_INTERFACE,
            rif_id,
            attr);

    return status;
}

/**
 * Routine Description:
 *    @brief Get router interface attribute
 *
 * Arguments:
 *    @param[in] rif_id - router interface id
 *    @param[in] attr_count - number of attributes
 *    @param[inout] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_get_router_interface_attribute(
    _In_ sai_object_id_t rif_id,
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

    if (local_router_interfaces_set.find(rif_id) == local_router_interfaces_set.end())
    {
        SWSS_LOG_ERROR("router interface %llx is missing", rif_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_get(
            SAI_OBJECT_TYPE_ROUTER_INTERFACE,
            rif_id,
            attr_count,
            attr_list);

    return status;
}

/**
 * @brief Routing interface methods table retrieved with sai_api_query()
 */
const sai_router_interface_api_t redis_router_interface_api = {
    redis_create_router_interface,
    redis_remove_router_interface,
    redis_set_router_interface_attribute,
    redis_get_router_interface_attribute,
};
