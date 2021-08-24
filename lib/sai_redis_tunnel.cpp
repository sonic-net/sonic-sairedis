#include "sai_redis.h"

REDIS_GENERIC_QUAD(TUNNEL_MAP,tunnel_map);
REDIS_GENERIC_QUAD(TUNNEL,tunnel);
REDIS_GENERIC_QUAD(TUNNEL_TERM_TABLE_ENTRY,tunnel_term_table_entry);
REDIS_GENERIC_QUAD(TUNNEL_MAP_ENTRY,tunnel_map_entry);
REDIS_GENERIC_STATS(TUNNEL,tunnel);

const sai_tunnel_api_t redis_tunnel_api = {

    REDIS_GENERIC_QUAD_API(tunnel_map)
    REDIS_GENERIC_QUAD_API(tunnel)
    REDIS_GENERIC_STATS_API(tunnel)
    REDIS_GENERIC_QUAD_API(tunnel_term_table_entry)
    REDIS_GENERIC_QUAD_API(tunnel_map_entry)
};
