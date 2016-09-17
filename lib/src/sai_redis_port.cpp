#include "sai_redis.h"

std::set<sai_object_id_t> local_ports_set;

/**
 * Routine Description:
 *   @brief Set port attribute value.
 *
 * Arguments:
 *    @param[in] port_id - port id
 *    @param[in] attr - attribute
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_set_port_attribute(
    _In_ sai_object_id_t port_id,
    _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr == NULL)
    {
        SWSS_LOG_ERROR("attribute parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    //TODO to make this work we need to populate port list first
    //since on current sai ports exist right away and are not created

    if (local_ports_set.find(port_id) == local_ports_set.end())
    {
        SWSS_LOG_ERROR("port %llx is missing", port_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    switch (attr->id)
    {
        case SAI_PORT_ATTR_SPEED:

            {
                uint32_t speed = attr->value.u32;

                switch (speed)
                {
                    default:

                        SWSS_LOG_ERROR("speed value is invalid :%u", speed);

                        return SAI_STATUS_INVALID_PARAMETER;
                }

                // TODO validate available speed values
            }

            break;

        case SAI_PORT_ATTR_ADMIN_STATE:
        case SAI_PORT_ATTR_OPER_STATUS:
            break;

        case SAI_PORT_ATTR_PORT_VLAN_ID:

            {
                sai_vlan_id_t vlan_id = attr->value.u16;

                if (local_vlans_set.find(vlan_id) == local_vlans_set.end())
                {
                    SWSS_LOG_ERROR("vlan %d is missing", vlan_id);

                    return SAI_STATUS_INVALID_PARAMETER;
                }
            }

            break;

        case SAI_PORT_ATTR_FDB_LEARNING:

            {
                sai_port_fdb_learning_mode_t mode = (sai_port_fdb_learning_mode_t)attr->value.s32;

                switch (mode)
                {
                    case SAI_PORT_LEARN_MODE_DROP:
                    case SAI_PORT_LEARN_MODE_DISABLE:
                    case SAI_PORT_LEARN_MODE_HW:
                    case SAI_PORT_LEARN_MODE_CPU_TRAP:
                    case SAI_PORT_LEARN_MODE_CPU_LOG:
                        // ok
                        break;

                    default:

                        SWSS_LOG_ERROR("invalid fdb learning mode value: %d", mode);

                        return SAI_STATUS_INVALID_PARAMETER;
                }

                break;
            }

        case SAI_PORT_ATTR_QOS_DSCP_TO_TC_MAP:
        case SAI_PORT_ATTR_QOS_TC_TO_QUEUE_MAP:
        case SAI_PORT_ATTR_QOS_TC_TO_PRIORITY_GROUP_MAP:
        case SAI_PORT_ATTR_QOS_PFC_PRIORITY_TO_PRIORITY_GROUP_MAP:
        case SAI_PORT_ATTR_QOS_PFC_PRIORITY_TO_QUEUE_MAP:

            {
                sai_object_id_t map_id = attr->value.oid;

                if (map_id == SAI_NULL_OBJECT_ID)
                {
                    // null object means to disable
                    break;
                }

                sai_object_type_t map_type = sai_object_type_query(map_id);

                // TODO look inside qos map object list if it exists

                if (map_type != SAI_OBJECT_TYPE_QOS_MAPS)
                {
                    SWSS_LOG_ERROR("dscp to tc map type is not SAI_OBJECT_TYPE_QOS_MAP: %d, id: %llx", map_type, map_id);

                    return SAI_STATUS_INVALID_PARAMETER;
                }

                // TODO additional validation may be required

                break;
            }

        //case SAI_PORT_ATTR_QOS_INGRESS_BUFFER_PROFILE_LIST: // TODO up to SAI_SWITCH_ATTR_INGRESS_BUFFER_POOL_NUM
        //case SAI_PORT_ATTR_QOS_EGRESS_BUFFER_PROFILE_LIST: // TODO up to SAI_SWITCH_ATTR_EGRESS_BUFFER_POOL_NUM

        case SAI_PORT_ATTR_PRIORITY_FLOW_CONTROL:

            {
                sai_uint8_t vector = attr->value.u8;

                SWSS_LOG_DEBUG("flow control vector 0x%x", vector);
            }

            break;

        default:

            SWSS_LOG_ERROR("setting attribute id %d is not supported", attr->id);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_set(
            SAI_OBJECT_TYPE_PORT,
            port_id,
            attr);

    return status;
}

/**
 * Routine Description:
 *   @brief Get port attribute value.
 *
 * Arguments:
 *    @param[in] port_id - port id
 *    @param[in] attr_count - number of attributes
 *    @param[inout] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_get_port_attribute(
    _In_ sai_object_id_t port_id,
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

    // TODO to make this work we need to populate port list first
    // since on current sai ports exist right away and are not created

    if (local_ports_set.find(port_id) == local_ports_set.end())
    {
        SWSS_LOG_ERROR("port %llx is missing", port_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        sai_attribute_t* attr = &attr_list[i];

        switch (attr->id)
        {
            case SAI_PORT_ATTR_HW_LANE_LIST:

                {
                    sai_u32_list_t lane_list = attr->value.u32list;

                    if (lane_list.list == NULL)
                    {
                        SWSS_LOG_ERROR("lane list is null in attribute %d", attr->id);

                        return SAI_STATUS_INVALID_PARAMETER;
                    }
                }

                break;

            default:

                SWSS_LOG_ERROR("getting attribute id %d is not supported", attr->id);

                return SAI_STATUS_INVALID_PARAMETER;
        }
    }


    sai_status_t status = redis_generic_get(
            SAI_OBJECT_TYPE_PORT,
            port_id,
            attr_count,
            attr_list);

    return status;
}

/**
 * Routine Description:
 *   @brief Get port statistics counters.
 *
 * Arguments:
 *    @param[in] port_id - port id
 *    @param[in] counter_ids - specifies the array of counter ids
 *    @param[in] number_of_counters - number of counters in the array
 *    @param[out] counters - array of resulting counter values.
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_get_port_stats(
    _In_ sai_object_id_t port_id,
    _In_ const sai_port_stat_counter_t *counter_ids,
    _In_ uint32_t number_of_counters,
    _Out_ uint64_t* counters)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

/**
 * Routine Description:
 *   @brief Clear port statistics counters.
 *
 * Arguments:
 *    @param[in] port_id - port id
 *    @param[in] counter_ids - specifies the array of counter ids
 *    @param[in] number_of_counters - number of counters in the array
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_clear_port_stats(
    _In_ sai_object_id_t port_id,
    _In_ const sai_port_stat_counter_t *counter_ids,
    _In_ uint32_t number_of_counters)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

/**
 * Routine Description:
 *   @brief Clear port's all statistics counters.
 *
 * Arguments:
 *    @param[in] port_id - port id
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_clear_port_all_stats(
    _In_ sai_object_id_t port_id)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

/**
 * Routine Description:
 *   Port state change notification
 *   Passed as a parameter into sai_initialize_switch()
 *
 * Arguments:
 *   @param[in] count - number of notifications
 *   @param[in] data - array of port operational status
 *
 * Return Values:
 *    None
 */
void redis_port_state_change_notification(
    _In_ uint32_t count,
    _In_ sai_port_oper_status_notification_t *data)
{
    SWSS_LOG_ENTER();
}

/**
 * Routine Description:
 *   @brief Port event notification
 *
 * Arguments:
 *    @param[in] count - number of notifications
 *    @param[in] data - array of port events

 * Return Values:
 *    None
 */
void redis_port_event_notification(
    _In_ uint32_t count,
    _In_ sai_port_event_notification_t *data)
{
    SWSS_LOG_ENTER();
}

/**
 * @brief Port methods table retrieved with sai_api_query()
 */
const sai_port_api_t redis_port_api = {
    redis_set_port_attribute,
    redis_get_port_attribute,
    redis_get_port_stats,
    redis_clear_port_stats,
    redis_clear_port_all_stats,
};
