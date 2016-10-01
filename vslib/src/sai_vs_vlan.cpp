#include "sai_vs.h"

sai_status_t vs_create_vlan(
        _In_ sai_vlan_id_t vlan_id)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_create_vlan(
            vlan_id,
            &vs_generic_create_vlan);
}

sai_status_t vs_remove_vlan(
        _In_ sai_vlan_id_t vlan_id)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_remove_vlan(
            vlan_id,
            &vs_generic_remove_vlan);
}

sai_status_t vs_set_vlan_attribute(
        _In_ sai_vlan_id_t vlan_id,
        _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_set_vlan(
            vlan_id,
            attr,
            &vs_generic_set_vlan);
}

sai_status_t vs_get_vlan_attribute(
        _In_ sai_vlan_id_t vlan_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_get_vlan(
            vlan_id,
            attr_count,
            attr_list,
            &vs_generic_get_vlan);
}

sai_status_t vs_create_vlan_member(
        _Out_ sai_object_id_t* vlan_member_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_create_oid(
            SAI_OBJECT_TYPE_VLAN_MEMBER,
            vlan_member_id,
            attr_count,
            attr_list,
            &vs_generic_create);
}

sai_status_t vs_remove_vlan_member(
        _In_ sai_object_id_t vlan_member_id)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_remove_oid(
            SAI_OBJECT_TYPE_VLAN_MEMBER,
            vlan_member_id,
            &vs_generic_remove);
}

sai_status_t vs_set_vlan_member_attribute(
        _In_ sai_object_id_t vlan_member_id,
        _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_set_oid(
            SAI_OBJECT_TYPE_VLAN_MEMBER,
            vlan_member_id,
            attr,
            &vs_generic_set);
}

sai_status_t vs_get_vlan_member_attribute(
        _In_ sai_object_id_t vlan_member_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_get_oid(
            SAI_OBJECT_TYPE_VLAN_MEMBER,
            vlan_member_id,
            attr_count,
            attr_list,
            &vs_generic_get);
}

sai_status_t vs_get_vlan_stats(
        _In_ sai_vlan_id_t vlan_id,
        _In_ const sai_vlan_stat_counter_t *counter_ids,
        _In_ uint32_t number_of_counters,
        _Out_ uint64_t* counters)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t vs_clear_vlan_stats(
        _In_ sai_vlan_id_t vlan_id,
        _In_ const sai_vlan_stat_counter_t *counter_ids,
        _In_ uint32_t number_of_counters)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

const sai_vlan_api_t vs_vlan_api = {
    vs_create_vlan,
    vs_remove_vlan,
    vs_set_vlan_attribute,
    vs_get_vlan_attribute,
    vs_create_vlan_member,
    vs_remove_vlan_member,
    vs_set_vlan_member_attribute,
    vs_get_vlan_member_attribute,
    vs_get_vlan_stats,
    vs_clear_vlan_stats,
};
