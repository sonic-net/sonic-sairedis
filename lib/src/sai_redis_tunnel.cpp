#include "sai_redis.h"

sai_status_t redis_clear_tunnel_stats(
        _In_ sai_object_id_t tunnel_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_tunnel_stat_t *counter_ids)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}


REDIS_GENERIC_QUAD(TUNNEL_MAP,tunnel_map);
REDIS_GENERIC_QUAD(TUNNEL,tunnel);
REDIS_GENERIC_QUAD(TUNNEL_TERM_TABLE_ENTRY,tunnel_term_table_entry);
REDIS_GENERIC_QUAD(TUNNEL_MAP_ENTRY,tunnel_map_entry);

REDIS_GENERIC_GET_STATS(TUNNEL, tunnel);

const sai_tunnel_api_t redis_tunnel_api = {
    REDIS_GENERIC_QUAD_API(tunnel_map)
    REDIS_GENERIC_QUAD_API(tunnel)
    
    redis_get_tunnel_stats,
    redis_clear_tunnel_stats,

    REDIS_GENERIC_QUAD_API(tunnel_term_table_entry)
    REDIS_GENERIC_QUAD_API(tunnel_map_entry)
};
