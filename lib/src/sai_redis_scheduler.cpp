#include "sai_redis.h"

std::set<sai_object_id_t> local_schedulers_set;

/**
 * @brief Create Scheduler Profile
 *
 * @param[out] scheduler_id Scheduler id
 * @param[in] attr_count number of attributes
 * @param[in] attr_list array of attributes
 *
 * @return SAI_STATUS_SUCCESS on success
 *          Failure status code on error
 */
sai_status_t redis_create_scheduler_profile(
    _Out_ sai_object_id_t *scheduler_id,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    // no mandatory atributtes

    const sai_attribute_t* attr_scheduling_algorithm = redis_get_attribute_by_id(SAI_SCHEDULER_ATTR_SCHEDULING_ALGORITHM, attr_count, attr_list);
    const sai_attribute_t* attr_scheduling_weight = redis_get_attribute_by_id(SAI_SCHEDULER_ATTR_SCHEDULING_WEIGHT, attr_count, attr_list);
    const sai_attribute_t* attr_shaper_type = redis_get_attribute_by_id(SAI_SCHEDULER_ATTR_SHAPER_TYPE, attr_count, attr_list);

    if (attr_scheduling_algorithm != NULL)
    {
        sai_scheduling_type_t type = (sai_scheduling_type_t)attr_scheduling_algorithm->value.s32;

        switch (type)
        {
            case SAI_SCHEDULING_STRICT:
            case SAI_SCHEDULING_WRR:
            case SAI_SCHEDULING_DWRR:
                // ok
                break;

            default:

                SWSS_LOG_ERROR("invalid scheduling type value: %d", type);

                return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    if (attr_scheduling_weight != NULL)
    {
        uint8_t weight = attr_scheduling_weight->value.u8;

        if (weight < 1 || weight > 100)
        {
            SWSS_LOG_ERROR("invalid scheduling weight %u <%u..%u>", weight, 1, 100);

            return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    if (attr_shaper_type != NULL)
    {
        sai_meter_type_t meter_type = (sai_meter_type_t)attr_shaper_type->value.s32;

        switch (meter_type)
        {
            case SAI_METER_TYPE_PACKETS:
            case SAI_METER_TYPE_BYTES:
                break;

            default:

                SWSS_LOG_ERROR("invalid meter type value: %d", meter_type);

                return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    sai_status_t status = redis_generic_create(
            SAI_OBJECT_TYPE_SCHEDULER,
            scheduler_id,
            attr_count,
            attr_list);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("inserting scheduler %llx to local state", *scheduler_id);

        local_schedulers_set.insert(*scheduler_id);

        // TODO increase reference count for used object ids
    }

    return status;
}

/**
 * @brief Remove Scheduler profile
 *
 * @param[in] scheduler_id Scheduler id
 *
 * @return SAI_STATUS_SUCCESS on success
 *          Failure status code on error
 */
sai_status_t redis_remove_scheduler_profile(
    _In_ sai_object_id_t scheduler_id)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    // TODO check if it safe to remove scheduler

    if (local_schedulers_set.find(scheduler_id) == local_schedulers_set.end())
    {
        SWSS_LOG_ERROR("scheduler %llx is missing", scheduler_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_remove(
            SAI_OBJECT_TYPE_SCHEDULER,
            scheduler_id);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("erasing scheduler %llx from local state", scheduler_id);

        local_schedulers_set.erase(scheduler_id);
    }

    return status;
}

/**
 * @brief Set Scheduler Attribute
 *
 * @param[in] scheduler_id Scheduler id
 * @param[in] attr attribute to set
 *
 * @return SAI_STATUS_SUCCESS on success
 *          Failure status code on error
 */
sai_status_t redis_set_scheduler_attribute(
    _In_ sai_object_id_t scheduler_id,
    _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr == NULL)
    {
        SWSS_LOG_ERROR("attribute parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (local_schedulers_set.find(scheduler_id) == local_schedulers_set.end())
    {
        SWSS_LOG_ERROR("scheduler %llx is missing", scheduler_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    switch (attr->id)
    {
        case SAI_SCHEDULER_ATTR_SCHEDULING_ALGORITHM:

            {
                sai_scheduling_type_t type = (sai_scheduling_type_t)attr->value.s32;

                switch (type)
                {
                    case SAI_SCHEDULING_STRICT:
                    case SAI_SCHEDULING_WRR:
                    case SAI_SCHEDULING_DWRR:
                        // ok
                        break;

                    default:

                        SWSS_LOG_ERROR("invalid scheduling type value: %d", type);

                        return SAI_STATUS_INVALID_PARAMETER;
                }
            }

            break;

        case SAI_SCHEDULER_ATTR_SCHEDULING_WEIGHT:

            {
                uint8_t weight = attr->value.u8;

                if (weight < 1 || weight > 100)
                {
                    SWSS_LOG_ERROR("invalid scheduling weight %u <%u..%u>", weight, 1, 100);

                    return SAI_STATUS_INVALID_PARAMETER;
                }
            }

            break;

        case SAI_SCHEDULER_ATTR_SHAPER_TYPE:

            {
                sai_meter_type_t meter_type = (sai_meter_type_t)attr->value.s32;

                switch (meter_type)
                {
                    case SAI_METER_TYPE_PACKETS:
                    case SAI_METER_TYPE_BYTES:
                        break;

                    default:

                        SWSS_LOG_ERROR("invalid meter type value: %d", meter_type);

                        return SAI_STATUS_INVALID_PARAMETER;
                }
            }

            break;

        case SAI_SCHEDULER_ATTR_MIN_BANDWIDTH_RATE:
        case SAI_SCHEDULER_ATTR_MIN_BANDWIDTH_BURST_RATE:
        case SAI_SCHEDULER_ATTR_MAX_BANDWIDTH_RATE:
        case SAI_SCHEDULER_ATTR_MAX_BANDWIDTH_BURST_RATE:
            // ok
            break;

        default:

            SWSS_LOG_ERROR("setting attribute id %d is not supported", attr->id);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_set(
            SAI_OBJECT_TYPE_SCHEDULER,
            scheduler_id,
            attr);

    return status;
}

/**
 * @brief Get Scheduler attribute
 *
 * @param[in] scheduler_id - scheduler id
 * @param[in] attr_count - number of attributes
 * @param[inout] attr_list - array of attributes
 *
 * @return SAI_STATUS_SUCCESS on success
 *        Failure status code on error
 */

sai_status_t redis_get_scheduler_attribute(
    _In_ sai_object_id_t scheduler_id,
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

    if (local_schedulers_set.find(scheduler_id) == local_schedulers_set.end())
    {
        SWSS_LOG_ERROR("scheduler %llx is missing", scheduler_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        sai_attribute_t* attr = &attr_list[i];

        switch (attr->id)
        {
            case SAI_SCHEDULER_ATTR_SCHEDULING_ALGORITHM:
            case SAI_SCHEDULER_ATTR_SCHEDULING_WEIGHT:
            case SAI_SCHEDULER_ATTR_SHAPER_TYPE:
            case SAI_SCHEDULER_ATTR_MIN_BANDWIDTH_RATE:
            case SAI_SCHEDULER_ATTR_MIN_BANDWIDTH_BURST_RATE:
            case SAI_SCHEDULER_ATTR_MAX_BANDWIDTH_RATE:
            case SAI_SCHEDULER_ATTR_MAX_BANDWIDTH_BURST_RATE:
                // ok
                break;

            default:

                SWSS_LOG_ERROR("getting attribute id %d is not supported", attr->id);

                return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    sai_status_t status = redis_generic_get(
            SAI_OBJECT_TYPE_SCHEDULER,
            scheduler_id,
            attr_count,
            attr_list);

    return status;
}

/**
 * @brief Scheduler methods table retrieved with sai_api_query()
 */
const sai_scheduler_api_t redis_scheduler_api = {
    redis_create_scheduler_profile,
    redis_remove_scheduler_profile,
    redis_set_scheduler_attribute,
    redis_get_scheduler_attribute,
};
