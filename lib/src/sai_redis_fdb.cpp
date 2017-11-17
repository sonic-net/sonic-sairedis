#include "sai_redis.h"

sai_status_t redis_flush_fdb_entries(
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

REDIS_GENERIC_QUAD_ENTRY(FDB_ENTRY,fdb_entry);

const sai_fdb_api_t redis_fdb_api = {

    REDIS_GENERIC_QUAD_API(fdb_entry)

    redis_flush_fdb_entries,
};
