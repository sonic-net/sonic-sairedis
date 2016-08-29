#include "sai_redis.h"

std::set<sai_object_id_t> local_queues_set;

std::set<std::string> local_queue_keys_set;
std::unordered_map<sai_object_id_t, std::string> local_queue_keys_map;

#ifndef SAI_QUEUE_ATTR_INDEX
#define SAI_QUEUE_ATTR_INDEX 0x100 // TODO remove on new SAI
#endif

/**
 * Routine Description:
 *    @brief Create queue
 *
 * Arguments:
 *    @param[out] queue_id - queue id
 *    @param[in] attr_count - number of attributes
 *    @param[in] attr_list - array of attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 *
 */
sai_status_t redis_create_queue(
    _Out_ sai_object_id_t* queue_id,
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

        // SAI_QUEUE_ATTR_TYPE
        // SAI_QUEUE_ATTR_INDEX

        return SAI_STATUS_INVALID_PARAMETER;
    }

    const sai_attribute_t* attr_type = redis_get_attribute_by_id(SAI_QUEUE_ATTR_TYPE , attr_count, attr_list);
    const sai_attribute_t* attr_index = redis_get_attribute_by_id(SAI_QUEUE_ATTR_INDEX, attr_count, attr_list);

    if (attr_type == NULL)
    {
        SWSS_LOG_ERROR("missing type attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    sai_queue_type_t type = (sai_queue_type_t)attr_type->value.s32;

    switch (type)
    {
        case SAI_QUEUE_TYPE_ALL:
        case SAI_QUEUE_TYPE_UNICAST:
        case SAI_QUEUE_TYPE_MULTICAST:
            // ok
            break;

        default:

            SWSS_LOG_ERROR("invalid type attribute value: %d", type);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_index == NULL)
    {
        SWSS_LOG_ERROR("missing index attribute");

        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    uint8_t index = attr_index->value.u8;

    if (index > 16) // TODO see where we can get actual value
    {
        SWSS_LOG_ERROR("invalid queue index value: %u", index);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // TYPE and INDEX is key, we can make separate method for that

    std::string str_type;
    sai_serialize_primitive(type, str_type);

    std::string str_index;
    sai_serialize_primitive(index, str_index);

    std::string key = str_type + ":" + str_index;

    // extra key validation (could be generic)

    if (local_queue_keys_set.find(key) != local_queue_keys_set.end())
    {
        SWSS_LOG_ERROR("queue with type:index %d:%u key: %s already exists", type, index, key.c_str());

        return SAI_STATUS_INVALID_PARAMETER;
    }

    const sai_attribute_t* attr_wred_profile_id = redis_get_attribute_by_id(SAI_QUEUE_ATTR_WRED_PROFILE_ID, attr_count, attr_list);
    const sai_attribute_t* attr_buffer_profile_id = redis_get_attribute_by_id(SAI_QUEUE_ATTR_BUFFER_PROFILE_ID, attr_count, attr_list);
    const sai_attribute_t* attr_scheduler_profile_id = redis_get_attribute_by_id(SAI_QUEUE_ATTR_SCHEDULER_PROFILE_ID, attr_count, attr_list);

    // TODO scheduler and profile are assigned by default from some internal objects
    // they must be here set as mandatory_on create, or is it possible to disable scheduler or profile?

    // WRED

    sai_object_id_t wred_profile_id = SAI_NULL_OBJECT_ID;

    if (attr_wred_profile_id != NULL)
    {
        wred_profile_id = attr_wred_profile_id->value.oid;
    }

    if (wred_profile_id != SAI_NULL_OBJECT_ID &&
        local_wreds_set.find(wred_profile_id) == local_wreds_set.end())
    {
        SWSS_LOG_ERROR("wred profile id %llx is missing", wred_profile_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // BUFFER

    sai_object_id_t buffer_profile_id = SAI_NULL_OBJECT_ID;

    if (attr_buffer_profile_id != NULL)
    {
        buffer_profile_id = attr_buffer_profile_id->value.oid;
    }

    if (buffer_profile_id != SAI_NULL_OBJECT_ID &&
        local_buffer_profiles_set.find(buffer_profile_id) == local_buffer_profiles_set.end())
    {
        SWSS_LOG_ERROR("buffer profile id %llx is missing", buffer_profile_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // SCHEDULER

    sai_object_id_t scheduler_profile_id = SAI_NULL_OBJECT_ID;

    if (attr_scheduler_profile_id != NULL)
    {
        scheduler_profile_id = attr_scheduler_profile_id->value.oid;
    }

    if (scheduler_profile_id != SAI_NULL_OBJECT_ID &&
        local_schedulers_set.find(scheduler_profile_id) == local_schedulers_set.end())
    {
        SWSS_LOG_ERROR("scheduler profile id %llx is missing", scheduler_profile_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_create(
            SAI_OBJECT_TYPE_QUEUE,
            queue_id,
            attr_count,
            attr_list);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("inserting queue %llx to local state", *queue_id);

        local_queues_set.insert(*queue_id);

        local_queue_keys_set.insert(key);   // TODO could be reverse hash

        local_queue_keys_map[*queue_id] = key;

        // TODO increase reference count for used object ids
    }

    return status;
}

/**
 * Routine Description:
 *    @brief Remove queue
 *
 * Arguments:
 *    @param[in] queue_id - queue id
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_remove_queue(
    _In_ sai_object_id_t queue_id)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    // TODO check if it safe to remove queue
    // it may require some extra logic sine queues
    // are pointed by index
    // some internal queues may not be possible to remove

    if (local_queues_set.find(queue_id) == local_queues_set.end())
    {
        SWSS_LOG_ERROR("queue %llx is missing", queue_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    std::string key = local_queue_keys_map[queue_id];

    sai_status_t status = redis_generic_remove(
            SAI_OBJECT_TYPE_QUEUE,
            queue_id);

    if (status == SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_DEBUG("erasing queue %llx from local state", queue_id);

        // TODO decrease reference count

        local_queues_set.erase(queue_id);

        local_queue_keys_set.erase(key);

        local_queue_keys_map.erase(queue_id);
    }

    return status;
}

/**
 * @brief Set attribute to Queue
 * @param[in] queue_id queue id to set the attribute
 * @param[in] attr attribute to set
 *
 * @return SAI_STATUS_SUCCESS on success
 *           Failure status code on error
 */
sai_status_t redis_set_queue_attribute(
    _In_ sai_object_id_t queue_id,
    _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr == NULL)
    {
        SWSS_LOG_ERROR("attribute parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (local_queues_set.find(queue_id) == local_queues_set.end())
    {
        SWSS_LOG_ERROR("queue %llx is missing", queue_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // TODO double check if it's possible to change that attributes on the fly
    switch (attr->id)
    {
        case SAI_QUEUE_ATTR_WRED_PROFILE_ID:

            {
                sai_object_id_t wred_profile_id = attr->value.oid;

                if (wred_profile_id != SAI_NULL_OBJECT_ID &&
                    local_wreds_set.find(wred_profile_id) == local_wreds_set.end())
                {
                    SWSS_LOG_ERROR("wred profile id %llx is missing", wred_profile_id);

                    return SAI_STATUS_INVALID_PARAMETER;
                }
            }

            break;

        case SAI_QUEUE_ATTR_BUFFER_PROFILE_ID:

            {
                sai_object_id_t buffer_profile_id = attr->value.oid;

                if (buffer_profile_id != SAI_NULL_OBJECT_ID &&
                    local_buffer_profiles_set.find(buffer_profile_id) == local_buffer_profiles_set.end())
                {
                    SWSS_LOG_ERROR("buffer profile id %llx is missing", buffer_profile_id);

                    return SAI_STATUS_INVALID_PARAMETER;
                }
            }

            break;

        case SAI_QUEUE_ATTR_SCHEDULER_PROFILE_ID:

            {
                sai_object_id_t scheduler_profile_id = attr->value.oid;

                if (scheduler_profile_id != SAI_NULL_OBJECT_ID &&
                    local_schedulers_set.find(scheduler_profile_id) == local_schedulers_set.end())
                {
                    SWSS_LOG_ERROR("scheduler profile id %llx is missing", scheduler_profile_id);

                    return SAI_STATUS_INVALID_PARAMETER;
                }
            }

            break;

        // currently next hop don't have attributes that can be set
        default:

            SWSS_LOG_ERROR("setting attribute id %d is not supported", attr->id);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_status_t status = redis_generic_set(
            SAI_OBJECT_TYPE_QUEUE,
            queue_id,
            attr);

    return status;
}

/**
 * @brief Get attribute to Queue
 * @param[in] queue_id queue id to set the attribute
 * @param[in] attr_count number of attributes
 * @param[inout] attr_list Array of attributes
 *
 * @return SAI_STATUS_SUCCESS on success
 *           Failure status code on error
 */
sai_status_t redis_get_queue_attribute(
    _In_ sai_object_id_t queue_id,
    _In_ uint32_t        attr_count,
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

    if (local_queues_set.find(queue_id) == local_queues_set.end())
    {
        SWSS_LOG_ERROR("queue %llx is missing", queue_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        sai_attribute_t* attr = &attr_list[i];

        switch (attr->id)
        {
            case SAI_QUEUE_ATTR_TYPE:
            case SAI_QUEUE_ATTR_INDEX:
            case SAI_QUEUE_ATTR_WRED_PROFILE_ID:
            case SAI_QUEUE_ATTR_BUFFER_PROFILE_ID:
            case SAI_QUEUE_ATTR_SCHEDULER_PROFILE_ID:
                // ok
                break;

            default:

                SWSS_LOG_ERROR("getting attribute id %d is not supported", attr->id);

                return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    sai_status_t status = redis_generic_get(
            SAI_OBJECT_TYPE_QUEUE,
            queue_id,
            attr_count,
            attr_list);

    return status;
}

/**
 * @brief   Get queue statistics counters.
 *
 * @param[in] queue_id Queue id
 * @param[in] counter_ids specifies the array of counter ids
 * @param[in] number_of_counters number of counters in the array
 * @param[out] counters array of resulting counter values.
 *
 * @return SAI_STATUS_SUCCESS on success
 *         Failure status code on error
 */
sai_status_t redis_get_queue_stats(
    _In_ sai_object_id_t queue_id,
    _In_ const sai_queue_stat_counter_t *counter_ids,
    _In_ uint32_t number_of_counters,
    _Out_ uint64_t* counters)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

/**
 * @brief   Clear queue statistics counters.
 *
 * @param[in] queue_id Queue id
 * @param[in] counter_ids specifies the array of counter ids
 * @param[in] number_of_counters number of counters in the array
 *
 * @return SAI_STATUS_SUCCESS on success
 *         Failure status code on error
 */
sai_status_t redis_clear_queue_stats(
    _In_ sai_object_id_t queue_id,
    _In_ const sai_queue_stat_counter_t *counter_ids,
    _In_ uint32_t number_of_counters)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

/**
 * @brief Qos methods table retrieved with sai_api_query()
 */
const sai_queue_api_t redis_queue_api = {
    redis_set_queue_attribute,
    redis_get_queue_attribute,
    redis_get_queue_stats,
    redis_clear_queue_stats,
};
