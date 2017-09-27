#include "sai_redis.h"

sai_status_t redis_get_port_stats(
        _In_ sai_object_id_t port_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_port_stat_t *counter_ids,
        _Out_ uint64_t *counters)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t redis_clear_port_stats(
        _In_ sai_object_id_t port_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_port_stat_t *counter_ids)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t redis_clear_port_all_stats(
        _In_ sai_object_id_t port_id)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

REDIS_GENERIC_QUAD(PORT,port);

const sai_port_api_t redis_port_api = {

    REDIS_GENERIC_QUAD_API(port)

    redis_get_port_stats,
    redis_clear_port_stats,
    redis_clear_port_all_stats,
};
