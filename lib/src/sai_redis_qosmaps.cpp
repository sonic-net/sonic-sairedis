#include "sai_redis.h"

std::set<sai_object_id_t> local_qos_maps_set;

/**
 * @brief Create Qos Map
 *
 * @param[out] qos_map_id Qos Map Id
 * @param[in] attr_count number of attributes
 * @param[in] attr_list array of attributes
 *
 * @return SAI_STATUS_SUCCESS on success
 *          Failure status code on error
 */
sai_status_t redis_create_qos_map(
    _Out_ sai_object_id_t* qos_map_id,
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

        // SAI_QOS_MAP_ATTR_TYPE

        return SAI_STATUS_INVALID_PARAMETER;
    }

    const sai_attribute_t* attr_type = redis_get_attribute_by_id(SAI_QOS_MAP_ATTR_TYPE, attr_count, attr_list);

    if (attr_type == NULL)
    {
        SWSS_LOG_ERROR("missing type attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    sai_qos_map_type_t type = (sai_qos_map_type_t)attr_type->value.s32;

    switch (type)
    {
        case SAI_QOS_MAP_DOT1P_TO_TC:
        case SAI_QOS_MAP_DOT1P_TO_COLOR:
        case SAI_QOS_MAP_DSCP_TO_TC:
        case SAI_QOS_MAP_DSCP_TO_COLOR:
        case SAI_QOS_MAP_TC_TO_QUEUE:
        case SAI_QOS_MAP_TC_AND_COLOR_TO_DSCP:
        case SAI_QOS_MAP_TC_AND_COLOR_TO_DOT1P:
        case SAI_QOS_MAP_TC_TO_PRIORITY_GROUP:
        case SAI_QOS_MAP_PFC_PRIORITY_TO_PRIORITY_GROUP:
        case SAI_QOS_MAP_PFC_PRIORITY_TO_QUEUE:
            // ok
            break;

        default:

            SWSS_LOG_ERROR("invalid type attribute value: %d", type);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    // TODO is that CREATE_ONLY or CREATE_AND_SET ?
    const sai_attribute_t* attr_map_to_value_list = redis_get_attribute_by_id(SAI_QOS_MAP_ATTR_MAP_TO_VALUE_LIST, attr_count, attr_list);

    if (attr_map_to_value_list != NULL)
    {
        // TODO some extra validation may be required here

        sai_qos_map_list_t map_list = attr_map_to_value_list->value.qosmap;

        uint32_t count = map_list.count;

        sai_qos_map_t* list = map_list.list;

        if (list == NULL)
        {
            SWSS_LOG_ERROR("qos map list is NULL");

            return SAI_STATUS_INVALID_PARAMETER;
        }

        if (count < 1)
        {
            SWSS_LOG_ERROR("qos map count is zero");

            return SAI_STATUS_INVALID_PARAMETER;
        }

        SWSS_LOG_DEBUG("qos map count value: %u", count);

        for (uint32_t i = 0; i < count; ++i)
        {
           sai_qos_map_t qos_map = list[i];

            // TODO queue index needs validation

            switch (qos_map.key.color)
            {
                case SAI_PACKET_COLOR_GREEN:
                case SAI_PACKET_COLOR_YELLOW:
                case SAI_PACKET_COLOR_RED:
                    // ok
                    break;

                default:

                    SWSS_LOG_ERROR("qos map packet color invalid value: %d", qos_map.key.color);

                    return SAI_STATUS_INVALID_PARAMETER;
            }

            switch (qos_map.value.color)
            {
                case SAI_PACKET_COLOR_GREEN:
                case SAI_PACKET_COLOR_YELLOW:
                case SAI_PACKET_COLOR_RED:
                    // ok
                    break;

                default:

                    SWSS_LOG_ERROR("qos map packet color invalid value: %d", qos_map.value.color);

                    return SAI_STATUS_INVALID_PARAMETER;
            }
        }
    }

    sai_status_t status = redis_generic_create(
            SAI_OBJECT_TYPE_QOS_MAPS,
            qos_map_id,
            attr_count,
            attr_list);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("inserting qos map %llx to local state", *qos_map_id);

        local_queues_set.insert(*qos_map_id);

        // TODO increase reference count for used object ids
    }

    return status;
}

/**
 * @brief Remove Qos Map
 *
 * @param[in] qos_map_id Qos Map id to be removed.
 *
 * @return SAI_STATUS_SUCCESS on success
 *           Failure status code on error
 */
sai_status_t redis_remove_qos_map (
    _In_ sai_object_id_t qos_map_id)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    // TODO check if it safe to remove qos map

    if (local_qos_maps_set.find(qos_map_id) == local_qos_maps_set.end())
    {
        SWSS_LOG_ERROR("qos map %llx is missing", qos_map_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_remove(
            SAI_OBJECT_TYPE_QOS_MAPS,
            qos_map_id);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("erasing qos map %llx from local state", qos_map_id);

        // TODO decrease reference count

        local_queues_set.erase(qos_map_id);
    }

    return status;
}

/**
 * @brief Set attributes for qos map
 *
 * @param[in] qos_map_id Qos Map Id
 * @param[in] attr attribute to set
 *
 * @return SAI_STATUS_SUCCESS on success
 *          Failure status code on error
 */
sai_status_t redis_set_qos_map_attribute(
    _In_ sai_object_id_t qos_map_id,
    _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr == NULL)
    {
        SWSS_LOG_ERROR("attribute parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (local_qos_maps_set.find(qos_map_id) == local_qos_maps_set.end())
    {
        SWSS_LOG_ERROR("qos map %llx is missing", qos_map_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    switch (attr->id)
    {
        // TODO double check if it's possible to change that attributes on the fly
        // case SAI_QOS_MAP_ATTR_MAP_TO_VALUE_LIST:
        default:

            SWSS_LOG_ERROR("setting attribute id %d is not supported", attr->id);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_set(
            SAI_OBJECT_TYPE_QOS_MAPS,
            qos_map_id,
            attr);

    return status;
}

/**
 * @brief Get attrbutes of qos map
 *
 * @param[in] qos_map_id map id
 * @param[in] attr_count number of attributes
 * @param[inout] attr_list array of attributes
 *
 * @return SAI_STATUS_SUCCESS on success
 *        Failure status code on error
 */
sai_status_t redis_get_qos_map_attribute(
     _In_ sai_object_id_t qos_map_id,
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

    if (local_qos_maps_set.find(qos_map_id) == local_qos_maps_set.end())
    {
        SWSS_LOG_ERROR("qos map %llx is missing", qos_map_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        sai_attribute_t* attr = &attr_list[i];

        switch (attr->id)
        {
            case SAI_QOS_MAP_ATTR_TYPE:
                break;

            case SAI_QOS_MAP_ATTR_MAP_TO_VALUE_LIST:

                if (attr->value.qosmap.list == NULL)
                {
                    SWSS_LOG_ERROR("map to value list is NULL");

                    return SAI_STATUS_INVALID_PARAMETER;
                }

                break;

            default:

                SWSS_LOG_ERROR("getting attribute id %d is not supported", attr->id);

                return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    sai_status_t status = redis_generic_get(
            SAI_OBJECT_TYPE_QOS_MAPS,
            qos_map_id,
            attr_count,
            attr_list);

    return status;
}

/**
 * Qos Map methods table retrieved with sai_api_query()
 */
const sai_qos_map_api_t redis_qos_map_api = {
    redis_create_qos_map,
    redis_remove_qos_map,
    redis_set_qos_map_attribute,
    redis_get_qos_map_attribute,
};
