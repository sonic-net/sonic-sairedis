#include "sai_vs.h"
#include "sai_vs_internal.h"

sai_status_t vs_get_ingress_priority_group_stats(
        _In_ sai_object_id_t ingress_pg_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_ingress_priority_group_stat_t *counter_ids,
        _Out_ uint64_t *counters)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t vs_clear_ingress_priority_group_stats(
        _In_ sai_object_id_t ingress_pg_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_ingress_priority_group_stat_t *counter_ids)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t vs_get_buffer_pool_stats(
        _In_ sai_object_id_t pool_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_buffer_pool_stat_t *counter_ids,
        _Out_ uint64_t *counters)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t vs_clear_buffer_pool_stats(
        _In_ sai_object_id_t pool_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_buffer_pool_stat_t *counter_ids)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

VS_GENERIC_QUAD(BUFFER_POOL,buffer_pool);
VS_GENERIC_QUAD(INGRESS_PRIORITY_GROUP,ingress_priority_group);
VS_GENERIC_QUAD(BUFFER_PROFILE,buffer_profile);

const sai_buffer_api_t vs_buffer_api = {

    VS_GENERIC_QUAD_API(buffer_pool)

    vs_get_buffer_pool_stats,
    vs_clear_buffer_pool_stats,

    VS_GENERIC_QUAD_API(ingress_priority_group)

    vs_get_ingress_priority_group_stats,
    vs_clear_ingress_priority_group_stats,

    VS_GENERIC_QUAD_API(buffer_profile)
};
