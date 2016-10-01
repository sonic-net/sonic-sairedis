#include "sai_vs.h"

sai_status_t vs_create_route(
        _In_ const sai_unicast_route_entry_t* unicast_route_entry,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_create_route_entry(
            unicast_route_entry,
            attr_count,
            attr_list,
            &vs_generic_create_route_entry);
}

sai_status_t vs_remove_route(
        _In_ const sai_unicast_route_entry_t* unicast_route_entry)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_remove_route_entry(
            unicast_route_entry,
            &vs_generic_remove_route_entry);
}

sai_status_t vs_set_route_attribute(
        _In_ const sai_unicast_route_entry_t* unicast_route_entry,
        _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_set_route_entry(
            unicast_route_entry,
            attr,
            &vs_generic_set_route_entry);
}

sai_status_t vs_get_route_attribute(
        _In_ const sai_unicast_route_entry_t* unicast_route_entry,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_get_route_entry(
            unicast_route_entry,
            attr_count,
            attr_list,
            &vs_generic_get_route_entry);
}

const sai_route_api_t vs_route_api = {
    vs_create_route,
    vs_remove_route,
    vs_set_route_attribute,
    vs_get_route_attribute,
};
