#include "sai_proxy.h"

PROXY_GENERIC_QUAD_ENTRY(OUTBOUND_ROUTING_ENTRY, outbound_routing_entry);
PROXY_BULK_CREATE_ENTRY_EX(OUTBOUND_ROUTING_ENTRY, outbound_routing_entry, outbound_routing_entries);
PROXY_BULK_REMOVE_ENTRY_EX(OUTBOUND_ROUTING_ENTRY, outbound_routing_entry, outbound_routing_entries);

const sai_dash_outbound_routing_api_t proxy_dash_outbound_routing_api = {
    PROXY_GENERIC_QUAD_API(outbound_routing_entry)
    proxy_bulk_create_outbound_routing_entries,
    proxy_bulk_remove_outbound_routing_entries,
};
