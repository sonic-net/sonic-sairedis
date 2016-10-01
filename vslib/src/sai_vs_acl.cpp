#include "sai_vs.h"

sai_status_t vs_create_acl_table(
        _Out_ sai_object_id_t* acl_table_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_create_oid(
            SAI_OBJECT_TYPE_ACL_TABLE,
            acl_table_id,
            attr_count,
            attr_list,
            &vs_generic_create);
}

sai_status_t vs_remove_acl_table(
        _In_ sai_object_id_t acl_table_id)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_remove_oid(
            SAI_OBJECT_TYPE_ACL_TABLE,
            acl_table_id,
            &vs_generic_remove);
}

sai_status_t vs_set_acl_table_attribute(
        _In_ sai_object_id_t acl_table_id,
        _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_set_oid(
            SAI_OBJECT_TYPE_ACL_TABLE,
            acl_table_id,
            attr,
            &vs_generic_set);
}

sai_status_t vs_get_acl_table_attribute(
        _In_ sai_object_id_t acl_table_id,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_get_oid(
            SAI_OBJECT_TYPE_ACL_TABLE,
            acl_table_id,
            attr_count,
            attr_list,
            &vs_generic_get);
}

sai_status_t vs_create_acl_entry(
        _Out_ sai_object_id_t *acl_entry_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_create_oid(
            SAI_OBJECT_TYPE_ACL_ENTRY,
            acl_entry_id,
            attr_count,
            attr_list,
            &vs_generic_create);
}

sai_status_t vs_remove_acl_entry(
        _In_ sai_object_id_t acl_entry_id)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_remove_oid(
            SAI_OBJECT_TYPE_ACL_ENTRY,
            acl_entry_id,
            &vs_generic_remove);
}

sai_status_t vs_set_acl_entry_attribute(
        _In_ sai_object_id_t acl_entry_id,
        _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_set_oid(
            SAI_OBJECT_TYPE_ACL_ENTRY,
            acl_entry_id,
            attr,
            &vs_generic_set);
}

sai_status_t vs_get_acl_entry_attribute(
        _In_ sai_object_id_t acl_entry_id,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_get_oid(
            SAI_OBJECT_TYPE_ACL_ENTRY,
            acl_entry_id,
            attr_count,
            attr_list,
            &vs_generic_get);
}

sai_status_t vs_create_acl_counter(
        _Out_ sai_object_id_t *acl_counter_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_create_oid(
            SAI_OBJECT_TYPE_ACL_COUNTER,
            acl_counter_id,
            attr_count,
            attr_list,
            &vs_generic_create);
}

sai_status_t vs_remove_acl_counter(
        _In_ sai_object_id_t acl_counter_id)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_remove_oid(
            SAI_OBJECT_TYPE_ACL_COUNTER,
            acl_counter_id,
            &vs_generic_remove);
}

sai_status_t vs_set_acl_counter_attribute(
        _In_ sai_object_id_t acl_counter_id,
        _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_set_oid(
            SAI_OBJECT_TYPE_ACL_COUNTER,
            acl_counter_id,
            attr,
            &vs_generic_set);
}

sai_status_t vs_get_acl_counter_attribute(
        _In_ sai_object_id_t acl_counter_id,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_get_oid(
            SAI_OBJECT_TYPE_ACL_COUNTER,
            acl_counter_id,
            attr_count,
            attr_list,
            &vs_generic_get);
}

sai_status_t vs_create_acl_range(
        _Out_ sai_object_id_t* acl_range_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_create_oid(
            SAI_OBJECT_TYPE_ACL_RANGE,
            acl_range_id,
            attr_count,
            attr_list,
            &vs_generic_create);
}

sai_status_t vs_remove_acl_range(
        _In_ sai_object_id_t acl_range_id)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_remove_oid(
            SAI_OBJECT_TYPE_ACL_RANGE,
            acl_range_id,
            &vs_generic_remove);
}

sai_status_t vs_set_acl_range_attribute(
        _In_ sai_object_id_t acl_range_id,
        _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_set_oid(
            SAI_OBJECT_TYPE_ACL_RANGE,
            acl_range_id,
            attr,
            &vs_generic_set);
}

sai_status_t vs_get_acl_range_attribute(
        _In_ sai_object_id_t acl_range_id,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_get_oid(
            SAI_OBJECT_TYPE_ACL_RANGE,
            acl_range_id,
            attr_count,
            attr_list,
            &vs_generic_get);
}

const sai_acl_api_t vs_acl_api = {
    vs_create_acl_table,
    vs_remove_acl_table,
    vs_set_acl_table_attribute,
    vs_get_acl_table_attribute,

    vs_create_acl_entry,
    vs_remove_acl_entry,
    vs_set_acl_entry_attribute,
    vs_get_acl_entry_attribute,

    vs_create_acl_counter,
    vs_remove_acl_counter,
    vs_set_acl_counter_attribute,
    vs_get_acl_counter_attribute,

    vs_create_acl_range,
    vs_remove_acl_range,
    vs_set_acl_range_attribute,
    vs_get_acl_range_attribute,
};
