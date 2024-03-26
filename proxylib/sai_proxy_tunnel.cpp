#include "sai_proxy.h"

PROXY_GENERIC_QUAD(TUNNEL_MAP,tunnel_map);
PROXY_GENERIC_QUAD(TUNNEL,tunnel);
PROXY_GENERIC_QUAD(TUNNEL_TERM_TABLE_ENTRY,tunnel_term_table_entry);
PROXY_GENERIC_QUAD(TUNNEL_MAP_ENTRY,tunnel_map_entry);
PROXY_GENERIC_STATS(TUNNEL,tunnel);
PROXY_BULK_QUAD(TUNNEL,tunnels);

const sai_tunnel_api_t proxy_tunnel_api = {

    PROXY_GENERIC_QUAD_API(tunnel_map)
    PROXY_GENERIC_QUAD_API(tunnel)
    PROXY_GENERIC_STATS_API(tunnel)
    PROXY_GENERIC_QUAD_API(tunnel_term_table_entry)
    PROXY_GENERIC_QUAD_API(tunnel_map_entry)
    PROXY_BULK_QUAD_API(tunnels)
};
