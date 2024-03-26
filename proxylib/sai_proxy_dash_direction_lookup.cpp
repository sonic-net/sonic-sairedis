#include "sai_proxy.h"

PROXY_GENERIC_QUAD_ENTRY(DIRECTION_LOOKUP_ENTRY, direction_lookup_entry);
PROXY_BULK_CREATE_ENTRY_EX(DIRECTION_LOOKUP_ENTRY, direction_lookup_entry, direction_lookup_entries);
PROXY_BULK_REMOVE_ENTRY_EX(DIRECTION_LOOKUP_ENTRY, direction_lookup_entry, direction_lookup_entries);

const sai_dash_direction_lookup_api_t proxy_dash_direction_lookup_api = {
    PROXY_GENERIC_QUAD_API(direction_lookup_entry)
    proxy_bulk_create_direction_lookup_entries,
    proxy_bulk_remove_direction_lookup_entries,
};
