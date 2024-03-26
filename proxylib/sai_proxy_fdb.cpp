#include "sai_proxy.h"

static sai_status_t proxy_flush_fdb_entries(
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return proxy_sai->flushFdbEntries(
            switch_id,
            attr_count,
            attr_list);
}

PROXY_GENERIC_QUAD_ENTRY(FDB_ENTRY,fdb_entry);
PROXY_BULK_QUAD_ENTRY(FDB_ENTRY,fdb_entry);

const sai_fdb_api_t proxy_fdb_api = {

    PROXY_GENERIC_QUAD_API(fdb_entry)

    proxy_flush_fdb_entries,

    PROXY_BULK_QUAD_API(fdb_entry)
};
