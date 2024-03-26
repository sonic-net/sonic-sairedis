#include "sai_proxy.h"

static sai_status_t proxy_remove_all_neighbor_entries(
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

PROXY_BULK_QUAD_ENTRY(NEIGHBOR_ENTRY,neighbor_entry);
PROXY_GENERIC_QUAD_ENTRY(NEIGHBOR_ENTRY,neighbor_entry);

const sai_neighbor_api_t proxy_neighbor_api = {

    PROXY_GENERIC_QUAD_API(neighbor_entry)
    proxy_remove_all_neighbor_entries,

    PROXY_BULK_QUAD_API(neighbor_entry)
};
