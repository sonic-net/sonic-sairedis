#include "sai_vs.h"
#include "sai_vs_internal.h"

sai_status_t vs_get_queue_stats(
        _In_ sai_object_id_t queue_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_queue_stat_t *counter_ids,
        _Out_ uint64_t *counters)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t vs_clear_queue_stats(
        _In_ sai_object_id_t queue_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_queue_stat_t *counter_ids)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

VS_GENERIC_QUAD(QUEUE,queue);

const sai_queue_api_t vs_queue_api = {

    VS_GENERIC_QUAD_API(queue)

    vs_get_queue_stats,
    vs_clear_queue_stats,
};
