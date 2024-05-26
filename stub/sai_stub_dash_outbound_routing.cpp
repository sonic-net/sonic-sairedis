#include "sai_stub.h"

STUB_GENERIC_QUAD_ENTRY(OUTBOUND_ROUTING_ENTRY, outbound_routing_entry);
STUB_BULK_CREATE_ENTRY_EX(OUTBOUND_ROUTING_ENTRY, outbound_routing_entry, outbound_routing_entries);
STUB_BULK_REMOVE_ENTRY_EX(OUTBOUND_ROUTING_ENTRY, outbound_routing_entry, outbound_routing_entries);

const sai_dash_outbound_routing_api_t stub_dash_outbound_routing_api = {
    STUB_GENERIC_QUAD_API(outbound_routing_entry)
    stub_bulk_create_outbound_routing_entries,
    stub_bulk_remove_outbound_routing_entries,
};
