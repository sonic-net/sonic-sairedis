#include "sai_redis.h"

sai_status_t redis_get_policer_stats(
        _In_ sai_object_id_t policer_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_policer_stat_t *counter_ids,
        _Out_ uint64_t *counters)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t redis_clear_policer_stats(
        _In_ sai_object_id_t policer_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_policer_stat_t *counter_ids)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

REDIS_GENERIC_QUAD(POLICER,policer);

const sai_policer_api_t redis_policer_api = {

    REDIS_GENERIC_QUAD_API(policer)

    redis_get_policer_stats,
    redis_clear_policer_stats,
};
