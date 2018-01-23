#include "sai_vs.h"
#include "sai_vs_internal.h"

sai_status_t internal_vs_flush_fdb_entries(
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    // TODO implement actual flush (9 cases) with ntf generation (queue)
    // also update meta db here

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t vs_flush_fdb_entries(
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return meta_sai_flush_fdb_entries(
            switch_id,
            attr_count,
            attr_list,
            internal_vs_flush_fdb_entries);
}

VS_GENERIC_QUAD_ENTRY(FDB_ENTRY,fdb_entry);

const sai_fdb_api_t vs_fdb_api = {

    VS_GENERIC_QUAD_API(fdb_entry)

    vs_flush_fdb_entries,
};
