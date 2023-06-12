#include "sai_redis.h"

REDIS_GENERIC_QUAD_ENTRY(DIRECTION_LOOKUP_ENTRY, direction_lookup_entry);
REDIS_BULK_CREATE_ENTRY_EX(DIRECTION_LOOKUP_ENTRY, direction_lookup_entry, direction_lookup_entries);
REDIS_BULK_REMOVE_ENTRY_EX(DIRECTION_LOOKUP_ENTRY, direction_lookup_entry, direction_lookup_entries);

const sai_dash_direction_lookup_api_t redis_dash_direction_lookup_api = {
    REDIS_GENERIC_QUAD_API(direction_lookup_entry)
    redis_bulk_create_direction_lookup_entries,
    redis_bulk_remove_direction_lookup_entries,
};
