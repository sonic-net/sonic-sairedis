#include "sai_vs.h"

VS_GENERIC_QUAD(TUNNEL_MAP,tunnel_map);
VS_GENERIC_QUAD(TUNNEL,tunnel);
VS_GENERIC_QUAD(TUNNEL_TERM_TABLE_ENTRY,tunnel_term_table_entry);
VS_GENERIC_QUAD(TUNNEL_MAP_ENTRY,tunnel_map_entry);
VS_GENERIC_STATS(TUNNEL,tunnel);
VS_BULK_QUAD(TUNNEL,tunnels);

const sai_tunnel_api_t vs_tunnel_api = {

    VS_GENERIC_QUAD_API(tunnel_map)
    VS_GENERIC_QUAD_API(tunnel)
    VS_GENERIC_STATS_API(tunnel)
    VS_GENERIC_QUAD_API(tunnel_term_table_entry)
    VS_GENERIC_QUAD_API(tunnel_map_entry)
    VS_BULK_QUAD_API(tunnels)
};
