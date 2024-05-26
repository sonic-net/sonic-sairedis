#include "sai_stub.h"

STUB_GENERIC_QUAD(TUNNEL_MAP,tunnel_map);
STUB_GENERIC_QUAD(TUNNEL,tunnel);
STUB_GENERIC_QUAD(TUNNEL_TERM_TABLE_ENTRY,tunnel_term_table_entry);
STUB_GENERIC_QUAD(TUNNEL_MAP_ENTRY,tunnel_map_entry);
STUB_GENERIC_STATS(TUNNEL,tunnel);
STUB_BULK_QUAD(TUNNEL,tunnels);

const sai_tunnel_api_t stub_tunnel_api = {

    STUB_GENERIC_QUAD_API(tunnel_map)
    STUB_GENERIC_QUAD_API(tunnel)
    STUB_GENERIC_STATS_API(tunnel)
    STUB_GENERIC_QUAD_API(tunnel_term_table_entry)
    STUB_GENERIC_QUAD_API(tunnel_map_entry)
    STUB_BULK_QUAD_API(tunnels)
};
