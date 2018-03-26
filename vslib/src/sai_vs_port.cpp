#include "sai_vs.h"
#include "sai_vs_internal.h"

sai_status_t vs_clear_port_stats(
        _In_ sai_object_id_t port_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_port_stat_t *counter_ids)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t vs_clear_port_all_stats(
        _In_ sai_object_id_t port_id)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t vs_clear_port_pool_stats(
        _In_ sai_object_id_t port_pool_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_port_pool_stat_t *counter_ids)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}


VS_GENERIC_GET_STATS(PORT,port);
VS_GENERIC_GET_STATS(PORT_POOL,port_pool);

VS_GENERIC_QUAD(PORT,port);
VS_GENERIC_QUAD(PORT_POOL,port_pool);

const sai_port_api_t vs_port_api = {

    VS_GENERIC_QUAD_API(port)

    vs_get_port_stats,
    vs_clear_port_stats,
    vs_clear_port_all_stats,

    VS_GENERIC_QUAD_API(port_pool)

    vs_get_port_pool_stats,
    vs_clear_port_pool_stats
};
