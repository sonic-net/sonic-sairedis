#include "sai_stub.h"

static sai_status_t stub_flush_fdb_entries(
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return stub_sai->flushFdbEntries(
            switch_id,
            attr_count,
            attr_list);
}

STUB_GENERIC_QUAD_ENTRY(FDB_ENTRY,fdb_entry);
STUB_BULK_QUAD_ENTRY(FDB_ENTRY,fdb_entry);

const sai_fdb_api_t stub_fdb_api = {

    STUB_GENERIC_QUAD_API(fdb_entry)

    stub_flush_fdb_entries,

    STUB_BULK_QUAD_API(fdb_entry)
};
