#include "sai_redis.h"

sai_status_t redis_clear_port_stats(
        _In_ sai_object_id_t port_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_port_stat_t *counter_ids)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t redis_get_port_stats_ext(
        _In_ sai_object_id_t port_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_port_stat_t *counter_ids,
	_In_ sai_stats_mode_t mode,
	_Out_ uint64_t *counters)
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

sai_status_t redis_clear_port_pool_stats(
        _In_ sai_object_id_t port_pool_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_port_pool_stat_t *counter_ids)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t redis_get_port_pool_stats_ext(
        _In_ sai_object_id_t port_pool_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_port_pool_stat_t *counter_ids,
	_In_ sai_stats_mode_t mode,
	_Out_ uint64_t *counters)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}


REDIS_GENERIC_QUAD(PORT,port);
REDIS_GENERIC_QUAD(PORT_POOL,port_pool);

REDIS_GENERIC_GET_STATS(PORT, port);
REDIS_GENERIC_GET_STATS(PORT_POOL, port_pool);

const sai_port_api_t redis_port_api = {

    REDIS_GENERIC_QUAD_API(port)

    redis_get_port_stats,
    redis_get_port_stats_ext,
    redis_clear_port_stats,
    redis_clear_port_all_stats,

    REDIS_GENERIC_QUAD_API(port_pool)

    redis_get_port_pool_stats,
    redis_get_port_pool_stats_ext,
    redis_clear_port_pool_stats
};
