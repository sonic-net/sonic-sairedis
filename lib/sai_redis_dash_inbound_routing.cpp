#include "sai_redis.h"

REDIS_GENERIC_QUAD_ENTRY(INBOUND_ROUTING_ENTRY, inbound_routing_entry);
REDIS_BULK_CREATE_ENTRY_EX(INBOUND_ROUTING_ENTRY, inbound_routing_entry, inbound_routing_entries);
REDIS_BULK_REMOVE_ENTRY_EX(INBOUND_ROUTING_ENTRY, inbound_routing_entry, inbound_routing_entries);

const sai_dash_inbound_routing_api_t redis_dash_inbound_routing_api = {
    REDIS_GENERIC_QUAD_API(inbound_routing_entry)
    redis_bulk_create_inbound_routing_entries,
    redis_bulk_remove_inbound_routing_entries,
};
