#include "sai_redis.h"

sai_status_t redis_get_ingress_priority_group_stats(
        _In_ sai_object_id_t ingress_pg_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_ingress_priority_group_stat_t *counter_ids,
        _Out_ uint64_t *counters)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t redis_clear_ingress_priority_group_stats(
        _In_ sai_object_id_t ingress_pg_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_ingress_priority_group_stat_t *counter_ids)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t redis_get_buffer_pool_stats(
        _In_ sai_object_id_t pool_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_buffer_pool_stat_t *counter_ids,
        _Out_ uint64_t *counters)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t redis_clear_buffer_pool_stats(
        _In_ sai_object_id_t pool_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_buffer_pool_stat_t *counter_ids)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

REDIS_GENERIC_QUAD(BUFFER_POOL,buffer_pool);
REDIS_GENERIC_QUAD(INGRESS_PRIORITY_GROUP,ingress_priority_group);
REDIS_GENERIC_QUAD(BUFFER_PROFILE,buffer_profile);

const sai_buffer_api_t redis_buffer_api = {

    REDIS_GENERIC_QUAD_API(buffer_pool)

    redis_get_buffer_pool_stats,
    redis_clear_buffer_pool_stats,

    REDIS_GENERIC_QUAD_API(ingress_priority_group)

    redis_get_ingress_priority_group_stats,
    redis_clear_ingress_priority_group_stats,

    REDIS_GENERIC_QUAD_API(buffer_profile)
};
