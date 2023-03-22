#include "sai_vs.h"

VS_GENERIC_QUAD_ENTRY(OUTBOUND_ROUTING_ENTRY, outbound_routing_entry);
VS_BULK_CREATE_ENTRY_EX(OUTBOUND_ROUTING_ENTRY, outbound_routing_entry, outbound_routing_entries);
VS_BULK_REMOVE_ENTRY_EX(OUTBOUND_ROUTING_ENTRY, outbound_routing_entry, outbound_routing_entries);

const sai_dash_outbound_routing_api_t vs_dash_outbound_routing_api = {
    VS_GENERIC_QUAD_API(outbound_routing_entry)
    vs_bulk_create_outbound_routing_entries,
    vs_bulk_remove_outbound_routing_entries,
};
