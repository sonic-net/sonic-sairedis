#include "sai_redis.h"

static sai_status_t redis_remove_all_neighbor_entries(
        _In_ sai_object_id_t switch_id)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

REDIS_BULK_QUAD_ENTRY(NEIGHBOR_ENTRY,neighbor_entry);
REDIS_GENERIC_QUAD_ENTRY(NEIGHBOR_ENTRY,neighbor_entry);

const sai_neighbor_api_t redis_neighbor_api = {

    REDIS_GENERIC_QUAD_API(neighbor_entry)
    redis_remove_all_neighbor_entries,

    REDIS_BULK_QUAD_API(neighbor_entry)
};
