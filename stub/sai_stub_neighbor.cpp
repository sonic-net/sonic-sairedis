#include "sai_stub.h"

static sai_status_t stub_remove_all_neighbor_entries(
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

STUB_BULK_QUAD_ENTRY(NEIGHBOR_ENTRY,neighbor_entry);
STUB_GENERIC_QUAD_ENTRY(NEIGHBOR_ENTRY,neighbor_entry);

const sai_neighbor_api_t stub_neighbor_api = {

    STUB_GENERIC_QUAD_API(neighbor_entry)
    stub_remove_all_neighbor_entries,

    STUB_BULK_QUAD_API(neighbor_entry)
};
