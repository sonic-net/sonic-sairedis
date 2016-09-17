#include "sai_redis.h"

std::set<sai_object_id_t> local_policers_set;

/**
 * @brief Create Policer
 *
 * @param[out] policer_id - the policer id
 * @param[in] attr_count - number of attributes
 * @param[in] attr_list - array of attributes
 *
 * @return SAI_STATUS_SUCCESS on success
 *         Failure status code on error
 */
sai_status_t redis_create_policer(
    _Out_ sai_object_id_t *policer_id,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr_count < 2)
    {
        SWSS_LOG_ERROR("attribute count must be at least 2");

        // SAI_POLICER_ATTR_METER_TYPE
        // SAI_POLICER_ATTR_MODE

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_list == NULL)
    {
        SWSS_LOG_ERROR("attribute list is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    const sai_attribute_t* attr_type = redis_get_attribute_by_id(SAI_POLICER_ATTR_METER_TYPE, attr_count, attr_list);
    const sai_attribute_t* attr_mode = redis_get_attribute_by_id(SAI_POLICER_ATTR_MODE, attr_count, attr_list);

    const sai_attribute_t* attr_meter_type = redis_get_attribute_by_id(SAI_POLICER_ATTR_PIR, attr_count, attr_list);

    if (attr_type == NULL)
    {
        SWSS_LOG_ERROR("missing type attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    sai_meter_type_t type = (sai_meter_type_t)attr_type->value.s32;

    switch (type)
    {
        case SAI_METER_TYPE_PACKETS:
        case SAI_METER_TYPE_BYTES:
            // ok
            break;

        default:

            SWSS_LOG_ERROR("invalid type attribute value: %d", type);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_policer_mode_t mode = (sai_policer_mode_t)attr_mode->value.s32;

    switch (mode)
    {
        case SAI_POLICER_MODE_Sr_TCM:

            break;

        case SAI_POLICER_MODE_Tr_TCM:

            if (attr_meter_type == NULL)
            {
                SWSS_LOG_ERROR("missing meter type attribute");

                return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
            }

            break;

        case SAI_POLICER_MODE_STORM_CONTROL:

            break;

        default:

            SWSS_LOG_ERROR("invalid mode attribute value: %d", mode);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    // TODO validate rest of enums if they are in range

    sai_status_t status = redis_generic_create(
            SAI_OBJECT_TYPE_POLICER,
            policer_id,
            attr_count,
            attr_list);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("policer %llx to local state", *policer_id);

        local_policers_set.insert(*policer_id);

        // TODO increase reference count for used object ids
    }

    return status;
}

/**
 * @brief Delete policer
 *
 * @param[in] policer_id - Policer id
 *
 * @return SAI_STATUS_SUCCESS on success
 *          Failure status code on error
 */
sai_status_t redis_remove_policer(
        _In_ sai_object_id_t policer_id)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    // TODO check if it is safe to remove policer
    // check dependencies and references

    if (local_policers_set.find(policer_id) == local_policers_set.end())
    {
        SWSS_LOG_ERROR("policer %llx is missing", policer_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_remove(
            SAI_OBJECT_TYPE_POLICER,
            policer_id);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("erasing policer %llx from local state", policer_id);

        local_policers_set.erase(policer_id);
    }

    return status;
}

/**
 * @brief Set Policer attribute
 *
 * @param[in] policer_id - Policer id
 * @param[in] attr - attribute
 *
 * @return SAI_STATUS_SUCCESS on success
 *         Failure status code on error
 */
sai_status_t redis_set_policer_attribute(
    _In_ sai_object_id_t policer_id,
    _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr == NULL)
    {
        SWSS_LOG_ERROR("attribute parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (local_policers_set.find(policer_id) == local_policers_set.end())
    {
        SWSS_LOG_ERROR("policer %llx is missing", policer_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    switch (attr->id)
    {
        // TODO double check if it possible to modify on the fly
        case SAI_POLICER_ATTR_COLOR_SOURCE:
        case SAI_POLICER_ATTR_CBS:
        case SAI_POLICER_ATTR_CIR:
        case SAI_POLICER_ATTR_PBS:
        case SAI_POLICER_ATTR_PIR:
        case SAI_POLICER_ATTR_GREEN_PACKET_ACTION:
        case SAI_POLICER_ATTR_YELLOW_PACKET_ACTION:
        case SAI_POLICER_ATTR_RED_PACKET_ACTION:
        case SAI_POLICER_ATTR_ENABLE_COUNTER_LIST:
            // ok
            break;

        default:

            SWSS_LOG_ERROR("setting attribute id %d is not supported", attr->id);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_set(
            SAI_OBJECT_TYPE_POLICER,
            policer_id,
            attr);

    return status;
}

/**
 * @brief Get Policer attribute
 *
 * @param[in] policer_id - policer id
 * @param[in] attr_count - number of attributes
 * @param[inout] attr_list - array of attributes
 *
 * @return SAI_STATUS_SUCCESS on success
 *         Failure status code on error
 */
sai_status_t redis_get_policer_attribute(
    _In_ sai_object_id_t policer_id,
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

    if (local_policers_set.find(policer_id) == local_policers_set.end())
    {
        SWSS_LOG_ERROR("policer %llx is missing", policer_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        sai_attribute_t* attr = &attr_list[i];

        switch (attr->id)
        {
            case SAI_POLICER_ATTR_METER_TYPE:
            case SAI_POLICER_ATTR_MODE:
            case SAI_POLICER_ATTR_COLOR_SOURCE:
            case SAI_POLICER_ATTR_CBS:
            case SAI_POLICER_ATTR_CIR:
            case SAI_POLICER_ATTR_PBS:
            case SAI_POLICER_ATTR_PIR:
            case SAI_POLICER_ATTR_GREEN_PACKET_ACTION:
            case SAI_POLICER_ATTR_YELLOW_PACKET_ACTION:
            case SAI_POLICER_ATTR_RED_PACKET_ACTION:
            case SAI_POLICER_ATTR_ENABLE_COUNTER_LIST:
                // ok
                break;

            default:

                SWSS_LOG_ERROR("getting attribute id %d is not supported", attr->id);

                return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    sai_status_t status = redis_generic_get(
            SAI_OBJECT_TYPE_POLICER,
            policer_id,
            attr_count,
            attr_list);

    return status;
}

/**
 * @brief Get Policer Statistics
 *
 * @param[in] policer_id - policer id
 * @param[in] counter_ids - array of counter ids
 * @param[in] number_of_counters - number of counters in the array
 * @param[out] counters - array of resulting counter values.
 *
 * @return SAI_STATUS_SUCCESS on success
 *         Failure status code on error
 */
sai_status_t redis_get_policer_stats(
    _In_ sai_object_id_t policer_id,
    _In_ const sai_policer_stat_counter_t *counter_ids,
    _In_ uint32_t number_of_counters,
    _Out_ uint64_t* counters)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

/**
 * @brief Policer methods table retrieved with sai_api_query()
 */
const sai_policer_api_t redis_policer_api = {
    redis_create_policer,
    redis_remove_policer,
    redis_set_policer_attribute,
    redis_get_policer_attribute,
    redis_get_policer_stats,
};
