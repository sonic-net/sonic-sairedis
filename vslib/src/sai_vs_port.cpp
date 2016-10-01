#include "sai_vs.h"

sai_status_t vs_set_port_attribute(
        _In_ sai_object_id_t port_id,
        _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_set_oid(
            SAI_OBJECT_TYPE_PORT,
            port_id,
            attr,
            &vs_generic_set);
}

sai_status_t vs_get_port_attribute(
        _In_ sai_object_id_t port_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_get_oid(
            SAI_OBJECT_TYPE_PORT,
            port_id,
            attr_count,
            attr_list,
            &vs_generic_get);
}

sai_status_t vs_get_port_stats(
        _In_ sai_object_id_t port_id,
        _In_ const sai_port_stat_counter_t *counter_ids,
        _In_ uint32_t number_of_counters,
        _Out_ uint64_t* counters)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t vs_clear_port_stats(
        _In_ sai_object_id_t port_id,
        _In_ const sai_port_stat_counter_t *counter_ids,
        _In_ uint32_t number_of_counters)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t vs_clear_port_all_stats(
        _In_ sai_object_id_t port_id)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

const sai_port_api_t vs_port_api = {
    vs_set_port_attribute,
    vs_get_port_attribute,
    vs_get_port_stats,
    vs_clear_port_stats,
    vs_clear_port_all_stats,
};
