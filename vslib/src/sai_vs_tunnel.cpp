#include "sai_vs.h"

sai_status_t vs_create_tunnel_map(
        _Out_ sai_object_id_t* tunnel_map_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_create_oid(
            SAI_OBJECT_TYPE_TUNNEL_MAP,
            tunnel_map_id,
            attr_count,
            attr_list,
            &vs_generic_create);
}

sai_status_t vs_remove_tunnel_map(
        _In_ sai_object_id_t tunnel_map_id)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_remove_oid(
            SAI_OBJECT_TYPE_TUNNEL_MAP,
            tunnel_map_id,
            &vs_generic_remove);
}

sai_status_t vs_set_tunnel_map_attribute(
        _In_ sai_object_id_t tunnel_map_id,
        _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_set_oid(
            SAI_OBJECT_TYPE_TUNNEL_MAP,
            tunnel_map_id,
            attr,
            &vs_generic_set);
}

sai_status_t vs_get_tunnel_map_attribute(
        _In_ sai_object_id_t   tunnel_map_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_get_oid(
            SAI_OBJECT_TYPE_TUNNEL_MAP,
            tunnel_map_id,
            attr_count,
            attr_list,
            &vs_generic_get);
}

sai_status_t vs_create_tunnel(
        _Out_ sai_object_id_t* tunnel_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_create_oid(
            SAI_OBJECT_TYPE_TUNNEL,
            tunnel_id,
            attr_count,
            attr_list,
            &vs_generic_create);
}

sai_status_t vs_remove_tunnel(
        _In_ sai_object_id_t tunnel_id)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_remove_oid(
            SAI_OBJECT_TYPE_TUNNEL,
            tunnel_id,
            &vs_generic_remove);
}

sai_status_t vs_set_tunnel_attribute(
        _In_ sai_object_id_t tunnel_id,
        _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_set_oid(
            SAI_OBJECT_TYPE_TUNNEL,
            tunnel_id,
            attr,
            &vs_generic_set);
}

sai_status_t vs_get_tunnel_attribute(
        _In_ sai_object_id_t tunnel_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_get_oid(
            SAI_OBJECT_TYPE_TUNNEL,
            tunnel_id,
            attr_count,
            attr_list,
            &vs_generic_get);
}

sai_status_t vs_create_tunnel_term_table_entry (
        _Out_ sai_object_id_t* tunnel_term_table_entry_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_create_oid(
            SAI_OBJECT_TYPE_TUNNEL_TABLE_ENTRY,
            tunnel_term_table_entry_id,
            attr_count,
            attr_list,
            &vs_generic_create);
}

sai_status_t vs_remove_tunnel_term_table_entry (
        _In_ sai_object_id_t tunnel_term_table_entry_id)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_remove_oid(
            SAI_OBJECT_TYPE_TUNNEL_TABLE_ENTRY,
            tunnel_term_table_entry_id,
            &vs_generic_remove);
}

sai_status_t vs_set_tunnel_term_table_entry_attribute(
        _In_ sai_object_id_t tunnel_term_table_entry_id,
        _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_set_oid(
            SAI_OBJECT_TYPE_TUNNEL_TABLE_ENTRY,
            tunnel_term_table_entry_id,
            attr,
            &vs_generic_set);
}

sai_status_t vs_get_tunnel_term_table_entry_attribute(
        _In_ sai_object_id_t tunnel_term_table_entry_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_get_oid(
            SAI_OBJECT_TYPE_TUNNEL_TABLE_ENTRY,
            tunnel_term_table_entry_id,
            attr_count,
            attr_list,
            &vs_generic_get);
}

const sai_tunnel_api_t vs_tunnel_api = {
    vs_create_tunnel_map,
    vs_remove_tunnel_map,
    vs_set_tunnel_map_attribute,
    vs_get_tunnel_map_attribute,
    vs_create_tunnel,
    vs_remove_tunnel,
    vs_set_tunnel_attribute,
    vs_get_tunnel_attribute,
    vs_create_tunnel_term_table_entry,
    vs_remove_tunnel_term_table_entry,
    vs_set_tunnel_term_table_entry_attribute,
    vs_get_tunnel_term_table_entry_attribute,
};
