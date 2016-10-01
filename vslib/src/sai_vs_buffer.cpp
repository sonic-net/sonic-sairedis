#include "sai_vs.h"

sai_status_t vs_set_ingress_priority_group_attr(
        _In_ sai_object_id_t ingress_pg_id,
        _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_set_oid(
            SAI_OBJECT_TYPE_PRIORITY_GROUP,
            ingress_pg_id,
            attr,
            &vs_generic_set);
}

sai_status_t vs_get_ingress_priority_group_attr(
        _In_ sai_object_id_t ingress_pg_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_get_oid(
            SAI_OBJECT_TYPE_PRIORITY_GROUP,
            ingress_pg_id,
            attr_count,
            attr_list,
            &vs_generic_get);
}

sai_status_t vs_get_ingress_priority_group_stats(
        _In_ sai_object_id_t ingress_pg_id,
        _In_ const sai_ingress_priority_group_stat_counter_t *counter_ids,
        _In_ uint32_t number_of_counters,
        _Out_ uint64_t* counters)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t vs_clear_ingress_priority_group_stats(
        _In_ sai_object_id_t ingress_pg_id,
        _In_ const sai_ingress_priority_group_stat_counter_t *counter_ids,
        _In_ uint32_t number_of_counters)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t vs_create_buffer_pool(
        _Out_ sai_object_id_t* pool_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_create_oid(
            SAI_OBJECT_TYPE_BUFFER_POOL,
            pool_id,
            attr_count,
            attr_list,
            &vs_generic_create);
}

sai_status_t vs_remove_buffer_pool(
        _In_ sai_object_id_t pool_id)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_remove_oid(
            SAI_OBJECT_TYPE_BUFFER_POOL,
            pool_id,
            &vs_generic_remove);
}

sai_status_t vs_set_buffer_pool_attr(
        _In_ sai_object_id_t pool_id,
        _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_set_oid(
            SAI_OBJECT_TYPE_BUFFER_POOL,
            pool_id,
            attr,
            &vs_generic_set);
}

sai_status_t vs_get_buffer_pool_attr(
        _In_ sai_object_id_t pool_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_get_oid(
            SAI_OBJECT_TYPE_BUFFER_POOL,
            pool_id,
            attr_count,
            attr_list,
            &vs_generic_get);
}

sai_status_t vs_get_buffer_pool_stats(
        _In_ sai_object_id_t pool_id,
        _In_ const sai_buffer_pool_stat_counter_t *counter_ids,
        _In_ uint32_t number_of_counters,
        _Out_ uint64_t* counters)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t vs_create_buffer_profile(
        _Out_ sai_object_id_t* buffer_profile_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_create_oid(
            SAI_OBJECT_TYPE_BUFFER_PROFILE,
            buffer_profile_id,
            attr_count,
            attr_list,
            &vs_generic_create);
}

sai_status_t vs_remove_buffer_profile(
        _In_ sai_object_id_t buffer_profile_id)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_remove_oid(
            SAI_OBJECT_TYPE_BUFFER_PROFILE,
            buffer_profile_id,
            &vs_generic_remove);
}

sai_status_t vs_set_buffer_profile_attr(
        _In_ sai_object_id_t buffer_profile_id,
        _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_set_oid(
            SAI_OBJECT_TYPE_BUFFER_PROFILE,
            buffer_profile_id,
            attr,
            &vs_generic_set);
}

sai_status_t vs_get_buffer_profile_attr(
        _In_ sai_object_id_t buffer_profile_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_get_oid(
            SAI_OBJECT_TYPE_BUFFER_PROFILE,
            buffer_profile_id,
            attr_count,
            attr_list,
            &vs_generic_get);
}

const sai_buffer_api_t vs_buffer_api = {
    vs_create_buffer_pool,
    vs_remove_buffer_pool,
    vs_set_buffer_pool_attr,
    vs_get_buffer_pool_attr,
    vs_get_buffer_pool_stats,

    vs_set_ingress_priority_group_attr,
    vs_get_ingress_priority_group_attr,
    vs_get_ingress_priority_group_stats,
    vs_clear_ingress_priority_group_stats,

    vs_create_buffer_profile,
    vs_remove_buffer_profile,
    vs_set_buffer_profile_attr,
    vs_get_buffer_profile_attr,
};
