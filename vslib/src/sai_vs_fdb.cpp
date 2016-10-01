#include "sai_vs.h"

sai_status_t vs_create_fdb_entry(
        _In_ const sai_fdb_entry_t *fdb_entry,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_create_fdb_entry(
            fdb_entry,
            attr_count,
            attr_list,
            &vs_generic_create_fdb_entry);
}

sai_status_t vs_remove_fdb_entry(
        _In_ const sai_fdb_entry_t* fdb_entry)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_remove_fdb_entry(
            fdb_entry,
            &vs_generic_remove_fdb_entry);
}

sai_status_t vs_set_fdb_entry_attribute(
        _In_ const sai_fdb_entry_t* fdb_entry,
        _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_set_fdb_entry(
            fdb_entry,
            attr,
            &vs_generic_set_fdb_entry);
}

sai_status_t vs_get_fdb_entry_attribute(
        _In_ const sai_fdb_entry_t* fdb_entry,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_get_fdb_entry(
            fdb_entry,
            attr_count,
            attr_list,
            &vs_generic_get_fdb_entry);
}

sai_status_t vs_flush_fdb_entries(
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

const sai_fdb_api_t vs_fdb_api = {
    vs_create_fdb_entry,
    vs_remove_fdb_entry,
    vs_set_fdb_entry_attribute,
    vs_get_fdb_entry_attribute,
    vs_flush_fdb_entries,
};
