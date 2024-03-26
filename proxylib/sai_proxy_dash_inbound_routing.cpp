#include "sai_proxy.h"

PROXY_GENERIC_QUAD_ENTRY(INBOUND_ROUTING_ENTRY, inbound_routing_entry);
PROXY_BULK_CREATE_ENTRY_EX(INBOUND_ROUTING_ENTRY, inbound_routing_entry, inbound_routing_entries);
PROXY_BULK_REMOVE_ENTRY_EX(INBOUND_ROUTING_ENTRY, inbound_routing_entry, inbound_routing_entries);

const sai_dash_inbound_routing_api_t proxy_dash_inbound_routing_api = {
    PROXY_GENERIC_QUAD_API(inbound_routing_entry)
    proxy_bulk_create_inbound_routing_entries,
    proxy_bulk_remove_inbound_routing_entries,
};
