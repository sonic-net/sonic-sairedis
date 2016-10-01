#include "sai_vs.h"

sai_status_t vs_create_lag(
        _Out_ sai_object_id_t* lag_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_create_oid(
            SAI_OBJECT_TYPE_LAG,
            lag_id,
            attr_count,
            attr_list,
            &vs_generic_create);
}

sai_status_t vs_remove_lag(
        _In_ sai_object_id_t lag_id)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_remove_oid(
            SAI_OBJECT_TYPE_LAG,
            lag_id,
            &vs_generic_remove);
}

sai_status_t vs_set_lag_attribute(
        _In_ sai_object_id_t lag_id,
        _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_set_oid(
            SAI_OBJECT_TYPE_LAG,
            lag_id,
            attr,
            &vs_generic_set);
}

sai_status_t vs_get_lag_attribute(
        _In_ sai_object_id_t lag_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_get_oid(
            SAI_OBJECT_TYPE_LAG,
            lag_id,
            attr_count,
            attr_list,
            &vs_generic_get);
}

sai_status_t vs_create_lag_member(
        _Out_ sai_object_id_t* lag_member_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_create_oid(
            SAI_OBJECT_TYPE_LAG_MEMBER,
            lag_member_id,
            attr_count,
            attr_list,
            &vs_generic_create);
}

sai_status_t vs_remove_lag_member(
        _In_ sai_object_id_t lag_member_id)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_remove_oid(
            SAI_OBJECT_TYPE_LAG_MEMBER,
            lag_member_id,
            &vs_generic_remove);
}

sai_status_t vs_set_lag_member_attribute(
        _In_ sai_object_id_t lag_member_id,
        _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_set_oid(
            SAI_OBJECT_TYPE_LAG_MEMBER,
            lag_member_id,
            attr,
            &vs_generic_set);
}

sai_status_t vs_get_lag_member_attribute(
        _In_ sai_object_id_t lag_member_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_get_oid(
            SAI_OBJECT_TYPE_LAG_MEMBER,
            lag_member_id,
            attr_count,
            attr_list,
            &vs_generic_get);
}

const sai_lag_api_t vs_lag_api = {
    vs_create_lag,
    vs_remove_lag,
    vs_set_lag_attribute,
    vs_get_lag_attribute,
    vs_create_lag_member,
    vs_remove_lag_member,
    vs_set_lag_member_attribute,
    vs_get_lag_member_attribute,
};
