#include "sai_redis.h"
#include "sai_redis_internal.h"

sai_status_t redis_get_queue_stats(
        _In_ sai_object_id_t queue_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_queue_stat_t *counter_ids,
        _Out_ uint64_t *counters)
{
    MUTEX();

    SWSS_LOG_ENTER();

    if (number_of_counters < 1)
    {
        SWSS_LOG_ERROR("Expected at least 1 counter ID");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (counter_ids == NULL)
    {
        SWSS_LOG_ERROR("counter_ids is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (counters == NULL)
    {
        SWSS_LOG_ERROR("counters is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t redis_clear_queue_stats(
        _In_ sai_object_id_t queue_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_queue_stat_t *counter_ids)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

REDIS_GENERIC_QUAD(QUEUE,queue);

const sai_queue_api_t redis_queue_api = {

    REDIS_GENERIC_QUAD_API(queue)

    redis_get_queue_stats,
    redis_clear_queue_stats,
};
