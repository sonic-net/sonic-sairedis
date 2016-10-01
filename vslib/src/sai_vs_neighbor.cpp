#include "sai_vs.h"

sai_status_t vs_create_neighbor_entry(
        _In_ const sai_neighbor_entry_t* neighbor_entry,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_create_neighbor_entry(
            neighbor_entry,
            attr_count,
            attr_list,
            &vs_generic_create_neighbor_entry);
}

sai_status_t vs_remove_neighbor_entry(
        _In_ const sai_neighbor_entry_t* neighbor_entry)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_remove_neighbor_entry(
            neighbor_entry,
            &vs_generic_remove_neighbor_entry);
}

sai_status_t vs_set_neighbor_attribute(
        _In_ const sai_neighbor_entry_t* neighbor_entry,
        _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_set_neighbor_entry(
            neighbor_entry,
            attr,
            &vs_generic_set_neighbor_entry);
}

sai_status_t vs_get_neighbor_attribute(
        _In_ const sai_neighbor_entry_t* neighbor_entry,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_get_neighbor_entry(
            neighbor_entry,
            attr_count,
            attr_list,
            &vs_generic_get_neighbor_entry);
}

sai_status_t vs_remove_all_neighbor_entries(
        void)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

const sai_neighbor_api_t vs_neighbor_api = {
    vs_create_neighbor_entry,
    vs_remove_neighbor_entry,
    vs_set_neighbor_attribute,
    vs_get_neighbor_attribute,
    vs_remove_all_neighbor_entries,
};
