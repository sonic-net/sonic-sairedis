#include "sai_stub.h"

STUB_GENERIC_QUAD_ENTRY(DIRECTION_LOOKUP_ENTRY, direction_lookup_entry);
STUB_BULK_CREATE_ENTRY_EX(DIRECTION_LOOKUP_ENTRY, direction_lookup_entry, direction_lookup_entries);
STUB_BULK_REMOVE_ENTRY_EX(DIRECTION_LOOKUP_ENTRY, direction_lookup_entry, direction_lookup_entries);

const sai_dash_direction_lookup_api_t stub_dash_direction_lookup_api = {
    STUB_GENERIC_QUAD_API(direction_lookup_entry)
    stub_bulk_create_direction_lookup_entries,
    stub_bulk_remove_direction_lookup_entries,
};
