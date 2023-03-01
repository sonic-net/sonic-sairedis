#include "sai_vs.h"

VS_GENERIC_QUAD_ENTRY(DIRECTION_LOOKUP_ENTRY, direction_lookup_entry);
VS_BULK_CREATE_ENTRY_EX(DIRECTION_LOOKUP_ENTRY, direction_lookup_entry, direction_lookup_entries);
VS_BULK_REMOVE_ENTRY_EX(DIRECTION_LOOKUP_ENTRY, direction_lookup_entry, direction_lookup_entries);

const sai_dash_direction_lookup_api_t vs_dash_direction_lookup_api = {
    VS_GENERIC_QUAD_API(direction_lookup_entry)
    vs_bulk_create_direction_lookup_entries,
    vs_bulk_remove_direction_lookup_entries,
};
