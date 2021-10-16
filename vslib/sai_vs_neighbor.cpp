#include "sai_vs.h"

static sai_status_t vs_remove_all_neighbor_entries(
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

VS_GENERIC_QUAD_ENTRY(NEIGHBOR_ENTRY,neighbor_entry);

const sai_neighbor_api_t vs_neighbor_api = {

    VS_GENERIC_QUAD_API(neighbor_entry)

    vs_remove_all_neighbor_entries,
};
