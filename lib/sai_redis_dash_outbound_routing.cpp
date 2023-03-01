#include "sai_redis.h"

REDIS_GENERIC_QUAD_ENTRY(OUTBOUND_ROUTING_ENTRY, outbound_routing_entry);
REDIS_BULK_CREATE_ENTRY_EX(OUTBOUND_ROUTING_ENTRY, outbound_routing_entry, outbound_routing_entries);
REDIS_BULK_REMOVE_ENTRY_EX(OUTBOUND_ROUTING_ENTRY, outbound_routing_entry, outbound_routing_entries);

const sai_dash_outbound_routing_api_t redis_dash_outbound_routing_api = {
    REDIS_GENERIC_QUAD_API(outbound_routing_entry)
    redis_bulk_create_outbound_routing_entries,
    redis_bulk_remove_outbound_routing_entries,
};
