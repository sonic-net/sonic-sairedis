#include "sai_redis.h"

std::set<sai_object_id_t> local_tunnel_maps_set;
std::set<sai_object_id_t> local_tunnels_set;
std::set<sai_object_id_t> local_tunnel_term_table_entries_set;

/**
 * Routine Description:
 *    @brief Create tunnel map
 *
 * Arguments:
 *    @param[out] tunnel_map_id - tunnel map id
 *    @param[in] attr_count - number of attributes
 *    @param[in] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 *
 */
sai_status_t redis_create_tunnel_map(
    _Out_ sai_object_id_t* tunnel_map_id,
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
        SWSS_LOG_ERROR("attribute count must be at least 1");

        // SAI_TUNNEL_MAP_ATTR_TYPE

        return SAI_STATUS_INVALID_PARAMETER;
    }

    const sai_attribute_t* attr_type = redis_get_attribute_by_id(SAI_TUNNEL_MAP_ATTR_TYPE, attr_count, attr_list);
    const sai_attribute_t* attr_map_to_value_list = redis_get_attribute_by_id(SAI_TUNNEL_MAP_ATTR_MAP_TO_VALUE_LIST, attr_count, attr_list);

    if (attr_type == NULL)
    {
        SWSS_LOG_ERROR("attribute type is missing");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    sai_tunnel_map_type_t type = (sai_tunnel_map_type_t)attr_type->value.s32;

    // TODO check if additional validation is needed on those types
    switch (type)
    {
        case SAI_TUNNEL_MAP_OECN_TO_UECN:
        case SAI_TUNNEL_MAP_UECN_OECN_TO_OECN:
        case SAI_TUNNEL_MAP_VNI_TO_VLAN_ID:
        case SAI_TUNNEL_MAP_VLAN_ID_TO_VNI:
            // ok
            break;

        default:

            SWSS_LOG_ERROR("invalid SAI_TUNNEL_MAP_ATTR_TYPE value: %d", type);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_map_to_value_list != NULL)
    {
        const sai_tunnel_map_list_t* tunnel_map_list = &attr_map_to_value_list->value.tunnelmap;

        if (tunnel_map_list->list == NULL)
        {
            SWSS_LOG_ERROR("tunnel map list is NULL");

            return SAI_STATUS_INVALID_PARAMETER;
        }

        for (uint32_t i = 0; i < tunnel_map_list->count; ++i)
        {
            const sai_tunnel_map_t* tunnel_map = &tunnel_map_list->list[i];

            // TODO validate tunnel map
            SWSS_LOG_DEBUG("tunnel map pointer: %llx", tunnel_map);
        }
    }

    sai_status_t status = redis_generic_create(
            SAI_OBJECT_TYPE_TUNNEL_MAP,
            tunnel_map_id,
            attr_count,
            attr_list);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("inserting tunnel map %llx to local state", *tunnel_map_id);

        local_tunnel_maps_set.insert(*tunnel_map_id);

        // TODO increase reference count for used object ids
    }

    return status;
}

/**
 * Routine Description:
 *    @brief Remove tunnel map
 *
 * Arguments:
 *    @param[out] tunnel_map_id - tunnel map id
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 *
 */
sai_status_t redis_remove_tunnel_map(
    _In_ sai_object_id_t tunnel_map_id)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    // TODO check if it is safe to remove tunnel map

    if (local_tunnel_maps_set.find(tunnel_map_id) == local_tunnel_maps_set.end())
    {
        SWSS_LOG_ERROR("tunnel map %llx is missing", tunnel_map_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_remove(
            SAI_OBJECT_TYPE_TUNNEL_MAP,
            tunnel_map_id);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("erasing tunnel map %llx to local state", tunnel_map_id);

        local_tunnel_maps_set.erase(tunnel_map_id);

        // TODO increase reference count for used object ids
    }

    return status;
}

/**
 * Routine Description:
 *    @brief Set tunnel map attribute Value
 *
 * Arguments:
 *    @param[in] tunnel_map_id - tunnel map id
 *    @param[in] attr - attribute
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_set_tunnel_map_attribute(
    _In_ sai_object_id_t tunnel_map_id,
    _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr == NULL)
    {
        SWSS_LOG_ERROR("attribute parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (local_tunnel_maps_set.find(tunnel_map_id) == local_tunnel_maps_set.end())
    {
        SWSS_LOG_ERROR("tunnel map %llx is missing", tunnel_map_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    switch (attr->id)
    {
        case SAI_TUNNEL_MAP_ATTR_MAP_TO_VALUE_LIST:

            // TODO validate this use case

            break;

        default:

            SWSS_LOG_ERROR("set attribute id %d is not allowed", attr->id);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_set(
            SAI_OBJECT_TYPE_TUNNEL_MAP,
            tunnel_map_id,
            attr);

    return status;
}

/**
 * Routine Description:
 *    @brief Get tunnel map attribute Value
 *
 * Arguments:
 *    @param[in] tunnel_map_id - tunnel map id
 *    @param[in] attr - attribute
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_get_tunnel_map_attribute(
    _In_ sai_object_id_t   tunnel_map_id,
    _In_ uint32_t attr_count,
    _Inout_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    // TODO logic in GET api can be unified for all APIs
    // if metadata will be available

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

    if (local_tunnel_maps_set.find(tunnel_map_id) == local_tunnel_maps_set.end())
    {
        SWSS_LOG_ERROR("tunnel map %llx is missing", tunnel_map_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        sai_attribute_t* attr = &attr_list[i];

        switch (attr->id)
        {
            case SAI_TUNNEL_MAP_ATTR_TYPE:
            case SAI_TUNNEL_MAP_ATTR_MAP_TO_VALUE_LIST:
                // ok
                break;

            default:

                SWSS_LOG_ERROR("set attribute id %d is not allowed", attr->id);

                return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    sai_status_t status = redis_generic_get(
            SAI_OBJECT_TYPE_TUNNEL_MAP,
            tunnel_map_id,
            attr_count,
            attr_list);

    return status;
}

/**
 * Routine Description:
 *    @brief Create tunnel
 *
 * Arguments:
 *    @param[out] tunnel_id - tunnel id
 *    @param[in] attr_count - number of attributes
 *    @param[in] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 *
 */
sai_status_t redis_create_tunnel(
    _Out_ sai_object_id_t* tunnel_id,
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
        SWSS_LOG_ERROR("attribute count must be at least 1");

        // SAI_TUNNEL_ATTR_TYPE
        //
        // - contitional on create depending on type
        //
        // SAI_TUNNEL_ATTR_ENCAP_TTL_VAL
        // SAI_TUNNEL_ATTR_ENCAP_DSCP_VAL
        // SAI_TUNNEL_ATTR_ENCAP_GRE_KEY
        // SAI_TUNNEL_ATTR_DECAP_TTL_MODE
        // SAI_TUNNEL_ATTR_DECAP_DSCP_MODE

        return SAI_STATUS_INVALID_PARAMETER;
    }

    const sai_attribute_t* attr_type = redis_get_attribute_by_id(SAI_TUNNEL_ATTR_TYPE, attr_count, attr_list);

    const sai_attribute_t* attr_underlay_interface = redis_get_attribute_by_id(SAI_TUNNEL_ATTR_UNDERLAY_INTERFACE, attr_count, attr_list);
    const sai_attribute_t* attr_overlay_interface = redis_get_attribute_by_id(SAI_TUNNEL_ATTR_OVERLAY_INTERFACE, attr_count, attr_list);

    const sai_attribute_t* attr_encap_ttl_mode = redis_get_attribute_by_id(SAI_TUNNEL_ATTR_ENCAP_TTL_MODE, attr_count, attr_list);
    const sai_attribute_t* attr_encap_ttl_val = redis_get_attribute_by_id(SAI_TUNNEL_ATTR_ENCAP_TTL_VAL, attr_count, attr_list);
    const sai_attribute_t* attr_encap_dscp_mode = redis_get_attribute_by_id(SAI_TUNNEL_ATTR_ENCAP_DSCP_MODE, attr_count, attr_list);
    const sai_attribute_t* attr_encap_dscp_val = redis_get_attribute_by_id(SAI_TUNNEL_ATTR_ENCAP_DSCP_VAL, attr_count, attr_list);
    const sai_attribute_t* attr_encap_gre_key_valid = redis_get_attribute_by_id(SAI_TUNNEL_ATTR_ENCAP_GRE_KEY_VALID, attr_count, attr_list);
    const sai_attribute_t* attr_encap_gre_key = redis_get_attribute_by_id(SAI_TUNNEL_ATTR_ENCAP_GRE_KEY, attr_count, attr_list);
    const sai_attribute_t* attr_encap_ecn_mode = redis_get_attribute_by_id(SAI_TUNNEL_ATTR_ENCAP_ECN_MODE, attr_count, attr_list);
    const sai_attribute_t* attr_encap_mappers = redis_get_attribute_by_id(SAI_TUNNEL_ATTR_ENCAP_MAPPERS, attr_count, attr_list);

    const sai_attribute_t* attr_decap_ecn_mode = redis_get_attribute_by_id(SAI_TUNNEL_ATTR_DECAP_ECN_MODE, attr_count, attr_list);
    const sai_attribute_t* attr_decap_mappers = redis_get_attribute_by_id(SAI_TUNNEL_ATTR_DECAP_MAPPERS, attr_count, attr_list);
    const sai_attribute_t* attr_decap_ttl_mode = redis_get_attribute_by_id(SAI_TUNNEL_ATTR_DECAP_TTL_MODE, attr_count, attr_list);
    const sai_attribute_t* attr_decap_dscp_mode = redis_get_attribute_by_id(SAI_TUNNEL_ATTR_DECAP_DSCP_MODE, attr_count, attr_list);

    if (attr_type == NULL)
    {
        SWSS_LOG_ERROR("missing type attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    if (attr_underlay_interface == NULL)
    {
        SWSS_LOG_ERROR("missing underlay interface attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    sai_object_id_t uif_id = attr_underlay_interface->value.oid;

    if (uif_id == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_ERROR("underlay interface object id is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // TODO for GRE/VXLAN it may be different type
    if (local_router_interfaces_set.find(uif_id) == local_router_interfaces_set.end())
    {
        SWSS_LOG_ERROR("underlay interface %llx is missing");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_overlay_interface == NULL)
    {
        SWSS_LOG_ERROR("missing overlay interface attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    sai_object_id_t oif_id = attr_overlay_interface->value.oid;

    if (oif_id == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_ERROR("overlay interface object id is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // TODO for GRE/VXLAN it may be different type
    if (local_router_interfaces_set.find(oif_id) == local_router_interfaces_set.end())
    {
        SWSS_LOG_ERROR("overlay interface %llx is missing");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_tunnel_type_t type = (sai_tunnel_type_t)attr_type->value.s32;

    switch (type)
    {
        case SAI_TUNNEL_IPINIP:
        // case SAI_TUNNEL_IPINIP_GRE:  // will probably require change, SAI_TUNNEL_ATTR_ENCAP_GRE_KEY_VALID may not be needed
        // case SAI_TUNNEL_VXLAN:
        // case SAI_TUNNEL_MPLS:
            // ok
            break;

        default:

            SWSS_LOG_ERROR("invalid tunnel type value: %d", type);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    // TODO should underlay/overlay interface be mandatory params?

    sai_tunnel_ttl_mode_t encap_ttl_mode = SAI_TUNNEL_TTL_UNIFORM_MODEL; // default value

    if (attr_encap_ttl_mode != NULL)
    {
        encap_ttl_mode = (sai_tunnel_ttl_mode_t)attr_encap_ttl_mode->value.s32;
    }

    switch (encap_ttl_mode)
    {
        case SAI_TUNNEL_TTL_UNIFORM_MODEL:

            break;

        case SAI_TUNNEL_TTL_PIPE_MODEL:

            if (attr_encap_ttl_val == NULL)
            {
                SWSS_LOG_ERROR("missing encap ttl val attribute");

                return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
            }

            break;

        default:

            SWSS_LOG_ERROR("invalid encap ttl mode value specified: %d", encap_ttl_mode);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_tunnel_dscp_mode_t encap_dscp_mode = SAI_TUNNEL_DSCP_UNIFORM_MODEL; // default value

    if (attr_encap_dscp_mode != NULL)
    {
        encap_dscp_mode = (sai_tunnel_dscp_mode_t)attr_encap_dscp_mode->value.s32;
    }

    switch (encap_dscp_mode)
    {
        case SAI_TUNNEL_DSCP_UNIFORM_MODEL:

            break;

        case SAI_TUNNEL_DSCP_PIPE_MODEL:

            if (attr_encap_dscp_val == NULL)
            {
                SWSS_LOG_ERROR("missing encap dscp val attribute");

                return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
            }

            break;

        default:

            SWSS_LOG_ERROR("invalid encap dscp mode specified: %d", encap_dscp_mode);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    bool encap_gre_key_valid = false; // default value // TODO is false default ?

    if (attr_encap_gre_key != NULL)
    {
        encap_gre_key_valid = attr_encap_gre_key_valid->value.booldata;
    }

    if (encap_gre_key_valid)
    {
       if (attr_encap_gre_key == NULL)
       {
            SWSS_LOG_ERROR("missing encap gre key attribute");

            return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
       }
    }

    sai_tunnel_encap_ecn_mode_t encap_ecn_mode = SAI_TUNNEL_ENCAP_ECN_MODE_STANDARD;

    if (attr_encap_ecn_mode != NULL)
    {
        encap_ecn_mode = (sai_tunnel_encap_ecn_mode_t)attr_encap_ecn_mode->value.s32;
    }

    switch (encap_ecn_mode)
    {
        case SAI_TUNNEL_ENCAP_ECN_MODE_STANDARD:

            break;

        case SAI_TUNNEL_ENCAP_ECN_MODE_USER_DEFINED:

            if (attr_encap_mappers == NULL)
            {
                SWSS_LOG_ERROR("missing encap mappers attibute");

                return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
            }

            if (attr_encap_mappers->value.objlist.list == NULL)
            {
                SWSS_LOG_ERROR("encap mappers list is NULL");

                return SAI_STATUS_INVALID_PARAMETER;
            }

            // TODO validate object on that list! if they exist
            // shoud this list contain at least 1 element ? or can it be empty?
            // check for duplicates on list ?

            {
                sai_object_list_t list = attr_encap_mappers->value.objlist;

                for (uint32_t i = 0; list.count; i++)
                {
                    sai_object_id_t obj = list.list[i];

                    if (obj == SAI_NULL_OBJECT_ID)
                    {
                        SWSS_LOG_ERROR("encap ecn mapper list contain null object id");

                        return SAI_STATUS_INVALID_PARAMETER;
                    }

                    // TODO check type of objects and if they exist
                }
            }

            // TODO we must check more like oecn/uecn must be present

            break;

        default:

            SWSS_LOG_ERROR("invalid encap ecn mode specified: %d", encap_ecn_mode);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_tunnel_decap_ecn_mode_t decap_ecn_mode = SAI_TUNNEL_DECAP_ECN_MODE_STANDARD;

    if (attr_decap_ecn_mode != NULL)
    {
        decap_ecn_mode = (sai_tunnel_decap_ecn_mode_t)attr_decap_ecn_mode->value.s32;
    }

    switch (decap_ecn_mode)
    {
        case SAI_TUNNEL_DECAP_ECN_MODE_STANDARD:

            break;

        case SAI_TUNNEL_DECAP_ECN_MODE_COPY_FROM_OUTER:

            // TODO should outer be defined now as input attribute ?

            break;

        case SAI_TUNNEL_DECAP_ECN_MODE_USER_DEFINED:

            if (attr_decap_mappers == NULL)
            {
                SWSS_LOG_ERROR("missing decap mappers attribute");

                return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
            }

            if (attr_decap_mappers->value.objlist.list == NULL)
            {
                SWSS_LOG_ERROR("decap mappers list is NULL");

                return SAI_STATUS_INVALID_PARAMETER;
            }

            // TODO validate object on that list! if they exist
            // shoud this list contain at least 1 element ? or can it be empty?

            {
                sai_object_list_t list = attr_decap_mappers->value.objlist;

                // TODO make this to helper function
                for (uint32_t i = 0; list.count; i++)
                {
                    sai_object_id_t obj = list.list[i];

                    if (obj == SAI_NULL_OBJECT_ID)
                    {
                        SWSS_LOG_ERROR("decap ecn mapper list contain null object id");

                        return SAI_STATUS_INVALID_PARAMETER;
                    }

                    // TODO check type of objects and if they exist
                }
            }

            // TODO more validation my be needed oecn/oecn mapper must be present

            break;

        default:

            SWSS_LOG_ERROR("invalid decap ecn mode value: %d", decap_ecn_mode);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_tunnel_ttl_mode_t decap_ttl_mode = SAI_TUNNEL_TTL_UNIFORM_MODEL; // default value

    sai_tunnel_dscp_mode_t decap_dscp_mode = SAI_TUNNEL_DSCP_UNIFORM_MODEL; // default value

    // TODO sai spec is inconsisten here, if this is mandatory attribute on some condition,
    // then it cannot have default value, dscp mode and ttl mode

    switch (type)
    {
        case SAI_TUNNEL_IPINIP:
        // case SAI_TUNNEL_IPINIP_GRE: // for now not supported

            if (attr_decap_ttl_mode == NULL)
            {
                SWSS_LOG_ERROR("missing decap ttl mode attribute");

                return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
            }

            decap_ttl_mode = (sai_tunnel_ttl_mode_t)attr_decap_ttl_mode->value.s32;

            if (attr_decap_dscp_mode == NULL)
            {
                SWSS_LOG_ERROR("missing decap dscp mode attribute");

                return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
            }

            decap_dscp_mode = (sai_tunnel_dscp_mode_t)attr_decap_dscp_mode->value.s32;

            break;

        default:
            // ok
            break;
    }

    switch (decap_ttl_mode)
    {
        case SAI_TUNNEL_TTL_UNIFORM_MODEL:
        case SAI_TUNNEL_TTL_PIPE_MODEL:
            // ok
            break;

        default:

            SWSS_LOG_ERROR("invalid decap ttl mode value: %d", decap_ttl_mode);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    switch (decap_dscp_mode)
    {
        case SAI_TUNNEL_DSCP_UNIFORM_MODEL:
        case SAI_TUNNEL_DSCP_PIPE_MODEL:
            // ok
            break;

        default:

            SWSS_LOG_ERROR("invalid decap dscp mode value: %d", decap_dscp_mode);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_create(
            SAI_OBJECT_TYPE_TUNNEL,
            tunnel_id,
            attr_count,
            attr_list);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("inserting tunnel %llx to local state", *tunnel_id);

        local_tunnels_set.insert(*tunnel_id);

        // TODO increase reference count for used object ids
    }

    return status;
}

/**
 * Routine Description:
 *    @brief Remove tunnel
 *
 * Arguments:
 *    @param[out] tunnel_id - tunnel map
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 *
 */
sai_status_t redis_remove_tunnel(
    _In_ sai_object_id_t tunnel_id)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    // TODO check if tunnel can safely be removed, if it is not used
    // in any tubbel table entry or map

    if (local_tunnels_set.find(tunnel_id) == local_tunnels_set.end())
    {
        SWSS_LOG_ERROR("tunnel %llx is missing", tunnel_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_remove(
            SAI_OBJECT_TYPE_TUNNEL,
            tunnel_id);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("erasing tunnel %llx from local state", tunnel_id);

        local_tunnels_set.erase(tunnel_id);
    }

    return status;
}

/**
 * Routine Description:
 *    @brief Set tunnel attribute Value
 *
 * Arguments:
 *    @param[in] tunnel_id - tunnel id
 *    @param[in] attr - attribute
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_set_tunnel_attribute(
    _In_ sai_object_id_t tunnel_id,
    _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr == NULL)
    {
        SWSS_LOG_ERROR("attribute parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (local_tunnels_set.find(tunnel_id) == local_tunnels_set.end())
    {
        SWSS_LOG_ERROR("tunnel %llx is missing", tunnel_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    switch (attr->id)
    {
        case SAI_TUNNEL_ATTR_ENCAP_ECN_MODE:
        case SAI_TUNNEL_ATTR_ENCAP_MAPPERS:

            // TODO validate this use case

            break;

        case SAI_TUNNEL_ATTR_DECAP_ECN_MODE:
        case SAI_TUNNEL_ATTR_DECAP_MAPPERS:

            // TODO validate this use case

            break;

        default:

            SWSS_LOG_ERROR("set attribute id %d is not allowed", attr->id);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_set(
            SAI_OBJECT_TYPE_TUNNEL,
            tunnel_id,
            attr);

    return status;
}

/**
 * Routine Description:
 *    @brief Get tunnel attribute Value
 *
 * Arguments:
 *    @param[in] tunnel_id - tunnel id
 *    @param[in] attr_count - number of attributes
 *    @param[in] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_get_tunnel_attribute(
    _In_ sai_object_id_t tunnel_id,
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

    if (local_tunnels_set.find(tunnel_id) == local_tunnels_set.end())
    {
        SWSS_LOG_ERROR("tunnel %llx is missing", tunnel_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // TODO depending on configuration like ecn modes or tunnel type
    // some attributes may be valid to get, some not

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        sai_attribute_t* attr = &attr_list[i];

        switch (attr->id)
        {
            case SAI_TUNNEL_ATTR_TYPE:
            case SAI_TUNNEL_ATTR_UNDERLAY_INTERFACE:
            case SAI_TUNNEL_ATTR_OVERLAY_INTERFACE:
            case SAI_TUNNEL_ATTR_ENCAP_SRC_IP:
            case SAI_TUNNEL_ATTR_ENCAP_TTL_MODE:
            case SAI_TUNNEL_ATTR_ENCAP_TTL_VAL:
            case SAI_TUNNEL_ATTR_ENCAP_DSCP_MODE:
            case SAI_TUNNEL_ATTR_ENCAP_DSCP_VAL:
            case SAI_TUNNEL_ATTR_ENCAP_GRE_KEY_VALID:
            case SAI_TUNNEL_ATTR_ENCAP_GRE_KEY:
            case SAI_TUNNEL_ATTR_ENCAP_ECN_MODE:
            case SAI_TUNNEL_ATTR_ENCAP_MAPPERS:
            case SAI_TUNNEL_ATTR_DECAP_ECN_MODE:
            case SAI_TUNNEL_ATTR_DECAP_MAPPERS:
            case SAI_TUNNEL_ATTR_DECAP_TTL_MODE:
            case SAI_TUNNEL_ATTR_DECAP_DSCP_MODE:
                // ok
                break;

            default:

                SWSS_LOG_ERROR("getting attribute id %d is not supported", attr->id);

                return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    sai_status_t status = redis_generic_get(
            SAI_OBJECT_TYPE_TUNNEL,
            tunnel_id,
            attr_count,
            attr_list);

    return status;
}

/**
 * Routine Description:
 *    @brief Create tunnel term table
 *
 * Arguments:
 *    @param[out] tunnel_term_table_entry_id - tunnel term table entry id
 *    @param[in] attr_count - number of attributes
 *    @param[in] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 *
 */
sai_status_t redis_create_tunnel_term_table_entry (
    _Out_ sai_object_id_t* tunnel_term_table_entry_id,
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

    if (attr_count < 4)
    {
        SWSS_LOG_ERROR("attribute count must be at least 4");

        // SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_VR_ID
        // SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_TYPE
        // SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_DST_IP
        // SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_TUNNEL_TYPE
        //
        // - contitional on create depending on type
        // SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_SRC_IP
        // SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_ACTION_TUNNEL_ID    // TODO is this conditional param ?

        return SAI_STATUS_INVALID_PARAMETER;
    }

    const sai_attribute_t* attr_vr_id = redis_get_attribute_by_id(SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_VR_ID, attr_count, attr_list);
    const sai_attribute_t* attr_type = redis_get_attribute_by_id(SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_TYPE, attr_count, attr_list);
    const sai_attribute_t* attr_dst_ip = redis_get_attribute_by_id(SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_DST_IP, attr_count, attr_list);
    const sai_attribute_t* attr_tunnel_type = redis_get_attribute_by_id(SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_TUNNEL_TYPE, attr_count, attr_list);

    const sai_attribute_t* attr_src_ip = redis_get_attribute_by_id(SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_SRC_IP, attr_count, attr_list);
    const sai_attribute_t* attr_action_tunnel_id = redis_get_attribute_by_id(SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_ACTION_TUNNEL_ID, attr_count, attr_list);

    if (attr_vr_id == NULL)
    {
        SWSS_LOG_ERROR("missing virtual router attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    sai_object_id_t vr_id = attr_vr_id->value.oid;

    // TODO make this a method, and add local virtual router to list
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

    if (attr_type == NULL)
    {
        SWSS_LOG_ERROR("attribute type is missing");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    sai_tunnel_term_table_entry_type_t type = (sai_tunnel_term_table_entry_type_t)attr_type->value.s32;

    switch (type)
    {
        case SAI_TUNNEL_TERM_TABLE_ENTRY_P2P:

            if (attr_src_ip == NULL)
            {
                SWSS_LOG_ERROR("attribute source ip is missing (table entry P2P)");

                return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
            }

            break;

        case SAI_TUNNEL_TERM_TABLE_ENTRY_P2MP:
            // TODO validate, is validation needed here?

            break;

        default:

            SWSS_LOG_ERROR("invalid SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_TYPE value: %d", type);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_dst_ip == NULL)
    {
        SWSS_LOG_ERROR("missing destination ip attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    if (attr_tunnel_type == NULL)
    {
        SWSS_LOG_ERROR("missing tunnel type attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    sai_tunnel_type_t tunnel_type = (sai_tunnel_type_t)attr_tunnel_type->value.s32;

    // TODO an extra validation may be needed on tunnel type
    switch (tunnel_type)
    {
        case SAI_TUNNEL_IPINIP:
        // case SAI_TUNNEL_IPINIP_GRE:  // for now not supported
        // case SAI_TUNNEL_VXLAN:
        // case SAI_TUNNEL_MPLS:
            // ok
            break;

        default:

            SWSS_LOG_ERROR("invalid tunnel type value: %d", tunnel_type);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    // TODO check is this conditional attribute, maybe this action is only
    // required for ip in ip tunnel types

    if (attr_action_tunnel_id == NULL)
    {
        SWSS_LOG_ERROR("attribute action tunnel is is missing");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    sai_object_id_t action_tunnel_id = attr_action_tunnel_id->value.oid;

    if (local_tunnels_set.find(action_tunnel_id) == local_tunnels_set.end())
    {
        SWSS_LOG_ERROR("tunnel %llx is missing", action_tunnel_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // TODO additional checks may be required sinec this action tunnel id is used for
    // decap so maybe this tunnel must have special attributes on creation set

    sai_status_t status = redis_generic_create(
            SAI_OBJECT_TYPE_TUNNEL_TABLE_ENTRY,
            tunnel_term_table_entry_id,
            attr_count,
            attr_list);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("inserting tunnel term table entry %llx to local state", *tunnel_term_table_entry_id);

        local_tunnel_term_table_entries_set.insert(*tunnel_term_table_entry_id);

        // TODO increase reference count for used object ids
    }

    return status;
}

/**
 * Routine Description:
 *    @brief Remove tunnel term table
 *
 * Arguments:
 *    @param[out] tunnel_term_table_entry_id - tunnel term table entry id
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 *
 */
sai_status_t redis_remove_tunnel_term_table_entry (
    _In_ sai_object_id_t tunnel_term_table_entry_id)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    // TODO validate if this table entry can be removed safely
    // maybe first entire table must be removed

    if (local_tunnel_term_table_entries_set.find(tunnel_term_table_entry_id) == local_tunnel_term_table_entries_set.end())
    {
        SWSS_LOG_ERROR("tunnel term table entry %llx is missing", tunnel_term_table_entry_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_remove(
            SAI_OBJECT_TYPE_TUNNEL_TABLE_ENTRY,
            tunnel_term_table_entry_id);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("erasing tunnel term table entry %llx from local state", tunnel_term_table_entry_id);

        local_tunnel_term_table_entries_set.erase(tunnel_term_table_entry_id);
    }

    return status;
}

/**
 * Routine Description:
 *    @brief Set tunnel term table attribute Value
 *
 * Arguments:
 *    @param[in] tunnel_term_table_entry_id, - tunnel term table id
 *    @param[in] attr - attribute
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_set_tunnel_term_table_entry_attribute(
    _In_ sai_object_id_t tunnel_term_table_entry_id,
    _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr == NULL)
    {
        SWSS_LOG_ERROR("attribute parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (local_tunnel_term_table_entries_set.find(tunnel_term_table_entry_id) == local_tunnel_term_table_entries_set.end())
    {
        SWSS_LOG_ERROR("tunnel term table entry %llx is missing", tunnel_term_table_entry_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    switch (attr->id)
    {
        // currently there are no attributes that can be set on table entry
        default:

            SWSS_LOG_ERROR("set attribute id %d is not allowed", attr->id);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_set(
            SAI_OBJECT_TYPE_TUNNEL_TABLE_ENTRY,
            tunnel_term_table_entry_id,
            attr);

    return status;
}

/**
 * Routine Description:
 *    @brief Get tunnel term table attribute Value
 *
 * Arguments:
 *    @param[in] tunnel_term_table_entry_id, - tunnel term table id
 *    @param[in] attr - attribute
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_get_tunnel_term_table_entry_attribute(
    _In_ sai_object_id_t tunnel_term_table_entry_id,
    _In_ uint32_t attr_count,
    _Inout_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    // TODO logic in GET api can be unified for all APIs
    // if metadata will be available

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

    if (local_tunnel_term_table_entries_set.find(tunnel_term_table_entry_id) == local_tunnel_term_table_entries_set.end())
    {
        SWSS_LOG_ERROR("tunnel term table entry %llx is missing", tunnel_term_table_entry_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        sai_attribute_t* attr = &attr_list[i];

        switch (attr->id)
        {
            case SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_SRC_IP:

            // TODO src ip is conditional on type: SAI_TUNNEL_TERM_TABLE_ENTRY_P2P
            // so additional check could be added here

            case SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_VR_ID:
            case SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_TYPE:
            case SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_DST_IP:
            case SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_TUNNEL_TYPE:
            case SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_ACTION_TUNNEL_ID:
                // ok
                break;

            default:

                SWSS_LOG_ERROR("getting attribute id %d is not supported", attr->id);

                return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    sai_status_t status = redis_generic_get(
            SAI_OBJECT_TYPE_TUNNEL_TABLE_ENTRY,
            tunnel_term_table_entry_id,
            attr_count,
            attr_list);

    return status;
}

/**
 * @brief tunnel table methods, retrieved via sai_api_query()
 */
const sai_tunnel_api_t redis_tunnel_api = {
    redis_create_tunnel_map,
    redis_remove_tunnel_map,
    redis_set_tunnel_map_attribute,
    redis_get_tunnel_map_attribute,
    redis_create_tunnel,
    redis_remove_tunnel,
    redis_set_tunnel_attribute,
    redis_get_tunnel_attribute,
    redis_create_tunnel_term_table_entry,
    redis_remove_tunnel_term_table_entry,
    redis_set_tunnel_term_table_entry_attribute,
    redis_get_tunnel_term_table_entry_attribute,
};
