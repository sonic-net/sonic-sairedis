#include "sai_vs.h"

static sai_status_t vs_flush_fdb_entries(
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return vs_sai->flushFdbEntries(
            switch_id,
            attr_count,
            attr_list);
}

VS_GENERIC_QUAD_ENTRY(FDB_ENTRY,fdb_entry);
VS_BULK_QUAD_ENTRY(FDB_ENTRY, fdb_entry);

const sai_fdb_api_t vs_fdb_api = {

    VS_GENERIC_QUAD_API(fdb_entry)

    vs_flush_fdb_entries,

    VS_BULK_QUAD_API(fdb_entry)
};
