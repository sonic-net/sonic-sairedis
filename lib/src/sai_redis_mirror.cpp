#include "sai_redis.h"

std::set<sai_object_id_t> local_mirror_sessions_set;

/**
 * @brief Create mirror session.
 *
 * @param[out] session_id Port mirror session id
 * @param[in] attr_count Number of attributes
 * @param[in] attr_list Value of attributes
 * @return SAI_STATUS_SUCCESS if operation is successful otherwise a different
 *   error code is returned.
 */
sai_status_t redis_create_mirror_session(
        _Out_ sai_object_id_t *session_id,
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

    if (attr_count < 14)
    {
        SWSS_LOG_ERROR("attribute count must be at least 14");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    const sai_attribute_t* attr_type = redis_get_attribute_by_id(SAI_MIRROR_SESSION_ATTR_TYPE, attr_count, attr_list);
    const sai_attribute_t* attr_monitor_port = redis_get_attribute_by_id(SAI_MIRROR_SESSION_ATTR_MONITOR_PORT, attr_count, attr_list);
    // const sai_attribute_t* attr_turncate_size = redis_get_attribute_by_id(SAI_MIRROR_SESSION_ATTR_TRUNCATE_SIZE, attr_count, attr_list);
    // const sai_attribute_t* attr_tc = redis_get_attribute_by_id(SAI_MIRROR_SESSION_ATTR_TC, attr_count, attr_list);
    const sai_attribute_t* attr_vlan_tpid = redis_get_attribute_by_id(SAI_MIRROR_SESSION_ATTR_VLAN_TPID, attr_count, attr_list);
    const sai_attribute_t* attr_vlan_id = redis_get_attribute_by_id(SAI_MIRROR_SESSION_ATTR_VLAN_ID, attr_count, attr_list);
    const sai_attribute_t* attr_vlan_pri = redis_get_attribute_by_id(SAI_MIRROR_SESSION_ATTR_VLAN_PRI, attr_count, attr_list);
    const sai_attribute_t* attr_vlan_cfi = redis_get_attribute_by_id(SAI_MIRROR_SESSION_ATTR_VLAN_CFI, attr_count, attr_list);
    const sai_attribute_t* attr_encap_type = redis_get_attribute_by_id(SAI_MIRROR_SESSION_ATTR_ENCAP_TYPE, attr_count, attr_list);
    const sai_attribute_t* attr_iphdr_version = redis_get_attribute_by_id(SAI_MIRROR_SESSION_ATTR_IPHDR_VERSION, attr_count, attr_list);
    const sai_attribute_t* attr_tos = redis_get_attribute_by_id(SAI_MIRROR_SESSION_ATTR_TOS, attr_count, attr_list);
    // const sai_attribute_t* attr_ttl = redis_get_attribute_by_id(SAI_MIRROR_SESSION_ATTR_TTL, attr_count, attr_list);
    const sai_attribute_t* attr_src_ip_address = redis_get_attribute_by_id(SAI_MIRROR_SESSION_ATTR_SRC_IP_ADDRESS, attr_count, attr_list);
    const sai_attribute_t* attr_dst_ip_address = redis_get_attribute_by_id(SAI_MIRROR_SESSION_ATTR_DST_IP_ADDRESS, attr_count, attr_list);
    const sai_attribute_t* attr_src_mac_address = redis_get_attribute_by_id(SAI_MIRROR_SESSION_ATTR_SRC_MAC_ADDRESS, attr_count, attr_list);
    const sai_attribute_t* attr_dst_mac_address = redis_get_attribute_by_id(SAI_MIRROR_SESSION_ATTR_DST_MAC_ADDRESS, attr_count, attr_list);
    const sai_attribute_t* attr_gre_protocol_type = redis_get_attribute_by_id(SAI_MIRROR_SESSION_ATTR_GRE_PROTOCOL_TYPE, attr_count, attr_list);

    if (attr_type == NULL)
    {
        SWSS_LOG_ERROR("missing type attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    sai_mirror_type_t mirror_type = (sai_mirror_type_t)attr_type->value.s32;

    switch (mirror_type)
    {
        case SAI_MIRROR_TYPE_LOCAL:
        case SAI_MIRROR_TYPE_REMOTE:
        case SAI_MIRROR_TYPE_ENHANCED_REMOTE:
            // ok
            break;

        default:

            SWSS_LOG_ERROR("invalid mirror type value: %d", mirror_type);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_monitor_port == NULL)
    {
        SWSS_LOG_ERROR("missing monitor port attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    sai_object_id_t monitor_port = attr_monitor_port->value.oid;

    // TODO can it be lag ? or tunnel ?
    if (local_ports_set.find(monitor_port) == local_ports_set.end())
    {
        SWSS_LOG_ERROR("monitor port %llx is missing", monitor_port);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // TODO vlan related are valid on RSPAN or ERSPAN

    if (attr_vlan_tpid == NULL)
    {
        SWSS_LOG_ERROR("missing vlan tpid attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    if (attr_vlan_id == NULL)
    {
        SWSS_LOG_ERROR("missing vlan id attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    sai_vlan_id_t vlan_id = attr_vlan_id->value.u16;

    if (local_vlans_set.find(vlan_id) == local_vlans_set.end())
    {
        SWSS_LOG_ERROR("vlan %d is missing", vlan_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_vlan_pri == NULL)
    {
        SWSS_LOG_ERROR("missing vlan pri attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    if (attr_vlan_cfi == NULL)
    {
        SWSS_LOG_ERROR("missing vlan pri attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    // TODO below parameters are only valid on SAI_MIRROR_TYPE_ENHANCED_REMOTE

    if (attr_encap_type == NULL)
    {
        SWSS_LOG_ERROR("mising encap type attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    sai_erspan_encapsulation_type_t enc_type = (sai_erspan_encapsulation_type_t)attr_encap_type->value.s32;

    switch (enc_type)
    {
        case SAI_MIRROR_L3_GRE_TUNNEL:
            // ok
            break;

        default:

            SWSS_LOG_ERROR("invalid encapsulation type value %d", enc_type);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_iphdr_version == NULL)
    {
        SWSS_LOG_ERROR("missing ip header version attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    uint8_t iphdr_version = attr_iphdr_version->value.u8;

    switch (iphdr_version)
    {
        case 4:
        case 6:
            // ok
            break;

        default:

            SWSS_LOG_ERROR("invalid ip header version value: %u", iphdr_version);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_tos == NULL)
    {
        SWSS_LOG_ERROR("missinig tos attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    if (attr_src_ip_address == NULL)
    {
        SWSS_LOG_ERROR("missinig src ip address attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    if (attr_dst_ip_address == NULL)
    {
        SWSS_LOG_ERROR("missinig dst ip address attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    if (attr_src_mac_address == NULL)
    {
        SWSS_LOG_ERROR("missinig src mac address attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    if (attr_dst_mac_address == NULL)
    {
        SWSS_LOG_ERROR("missinig dst mac address attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    if (attr_gre_protocol_type == NULL)
    {
        SWSS_LOG_ERROR("missinig gre protocol type attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    // TODO validate GRE protocol type value

    sai_status_t status = redis_generic_create(
            SAI_OBJECT_TYPE_MIRROR,
            session_id,
            attr_count,
            attr_list);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("inserting mirror session %llx to local state", *session_id);

        local_mirror_sessions_set.insert(*session_id);

        // TODO increase reference count for used object ids
    }

    return status;
}

/**
 * @brief Remove mirror session.
 *
 * @param[in] session_id Port mirror session id
 * @return SAI_STATUS_SUCCESS if operation is successful otherwise a different
 *   error code is returned.
 */
sai_status_t redis_remove_mirror_session(
        _In_ sai_object_id_t session_id)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    // TODO check if it safe to remove mirror session

    if (local_mirror_sessions_set.find(session_id) == local_mirror_sessions_set.end())
    {
        SWSS_LOG_ERROR("mirror session %llx is missing", session_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_remove(
            SAI_OBJECT_TYPE_MIRROR,
            session_id);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("erasing mirror session %llx from local state", session_id);

        local_mirror_sessions_set.erase(session_id);
    }

    return status;
}

/**
 * @brief Set mirror session attributes.
 *
 * @param[in] session_id Port mirror session id
 * @param[in] attr Value of attribute
 * @return SAI_STATUS_SUCCESS if operation is successful otherwise a different
 *   error code is returned.
 */
sai_status_t redis_set_mirror_session_attribute(
        _In_ sai_object_id_t session_id,
        _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr == NULL)
    {
        SWSS_LOG_ERROR("attribute parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (local_mirror_sessions_set.find(session_id) == local_mirror_sessions_set.end())
    {
        SWSS_LOG_ERROR("mirror session %llx is missing", session_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // TODO we need to type to decide which parameters are safe to set

    switch (attr->id)
    {
        case SAI_MIRROR_SESSION_ATTR_MONITOR_PORT:

            // TODO should changing port during mirror session should be possible ?
            // what if new port is somehow incompattible with current session?

            {
                sai_object_id_t monitor_port = attr->value.oid;

                // TODO can it be lag ? or tunnel ?
                if (local_ports_set.find(monitor_port) == local_ports_set.end())
                {
                    SWSS_LOG_ERROR("monitor port %llx is missing", monitor_port);

                    return SAI_STATUS_INVALID_PARAMETER;
                }
            }

            break;

        case SAI_MIRROR_SESSION_ATTR_TRUNCATE_SIZE:
        case SAI_MIRROR_SESSION_ATTR_TC:
            break;

        // TODO those are only valid if type is RSPAN and ERSPAN

        case SAI_MIRROR_SESSION_ATTR_VLAN_TPID:
            break;

        case SAI_MIRROR_SESSION_ATTR_VLAN_ID:   // TODO check if vlan exist

            {
                sai_vlan_id_t vlan_id = attr->value.u16;

                if (local_vlans_set.find(vlan_id) == local_vlans_set.end())
                {
                    SWSS_LOG_ERROR("vlan %d is missing", vlan_id);

                    return SAI_STATUS_INVALID_PARAMETER;
                }
            }

            break;

        case SAI_MIRROR_SESSION_ATTR_VLAN_PRI:
        case SAI_MIRROR_SESSION_ATTR_VLAN_CFI:
                break;


        // TODO thsoe are only valid if type is ERSPAN
        case SAI_MIRROR_SESSION_ATTR_IPHDR_VERSION:
                // TODO validate version;
                break;
        case SAI_MIRROR_SESSION_ATTR_TOS:
        case SAI_MIRROR_SESSION_ATTR_TTL:
        case SAI_MIRROR_SESSION_ATTR_SRC_IP_ADDRESS:
        case SAI_MIRROR_SESSION_ATTR_DST_IP_ADDRESS:
        case SAI_MIRROR_SESSION_ATTR_SRC_MAC_ADDRESS:
        case SAI_MIRROR_SESSION_ATTR_DST_MAC_ADDRESS:
                break;
        case SAI_MIRROR_SESSION_ATTR_GRE_PROTOCOL_TYPE:
                // TODO validate GRE protocol
                break;

        default:

            SWSS_LOG_ERROR("setting attribute id %d is not supported", attr->id);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_set(
            SAI_OBJECT_TYPE_MIRROR,
            session_id,
            attr);

    return status;
}

/**
 * @brief Get mirror session attributes.
 *
 * @param[in] session_id Port mirror session id
 * @param[in] attr_count Number of attributes
 * @param[inout] attr_list Value of attribute
 * @return SAI_STATUS_SUCCESS if operation is successful otherwise a different
 *   error code is returned.
 */
sai_status_t redis_get_mirror_session_attribute(
        _In_ sai_object_id_t session_id,
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

    if (local_mirror_sessions_set.find(session_id) == local_mirror_sessions_set.end())
    {
        SWSS_LOG_ERROR("mirror session %llx is missing", session_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // TODO we need to get type to validate which GET attributes are supported

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        sai_attribute_t* attr = &attr_list[i];

        switch (attr->id)
        {
            case SAI_MIRROR_SESSION_ATTR_MONITOR_PORT:
            case SAI_MIRROR_SESSION_ATTR_TRUNCATE_SIZE:
            case SAI_MIRROR_SESSION_ATTR_TC:
                break;

            // TODO those are only valid if type is RSPAN and ERSPAN

            case SAI_MIRROR_SESSION_ATTR_VLAN_TPID:
            case SAI_MIRROR_SESSION_ATTR_VLAN_ID:
            case SAI_MIRROR_SESSION_ATTR_VLAN_PRI:
            case SAI_MIRROR_SESSION_ATTR_VLAN_CFI:
                    break;

            // TODO thsoe are only valid if type is ERSPAN

            case SAI_MIRROR_SESSION_ATTR_ENCAP_TYPE:
            case SAI_MIRROR_SESSION_ATTR_IPHDR_VERSION:
            case SAI_MIRROR_SESSION_ATTR_TOS:
            case SAI_MIRROR_SESSION_ATTR_TTL:
            case SAI_MIRROR_SESSION_ATTR_SRC_IP_ADDRESS:
            case SAI_MIRROR_SESSION_ATTR_DST_IP_ADDRESS:
            case SAI_MIRROR_SESSION_ATTR_SRC_MAC_ADDRESS:
            case SAI_MIRROR_SESSION_ATTR_DST_MAC_ADDRESS:
            case SAI_MIRROR_SESSION_ATTR_GRE_PROTOCOL_TYPE:
                break;

            default:

                SWSS_LOG_ERROR("getting attribute id %d is not supported", attr->id);

                return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    sai_status_t status = redis_generic_get(
            SAI_OBJECT_TYPE_MIRROR,
            session_id,
            attr_count,
            attr_list);

    return status;
}

/**
 * @brief MIRROR method table retrieved with sai_api_query()
 */
const sai_mirror_api_t redis_mirror_api = {
    redis_create_mirror_session,
    redis_remove_mirror_session,
    redis_set_mirror_session_attribute,
    redis_get_mirror_session_attribute,
};
