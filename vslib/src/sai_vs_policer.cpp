#include "sai_vs.h"

sai_status_t vs_create_policer(
        _Out_ sai_object_id_t *policer_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_create_oid(
            SAI_OBJECT_TYPE_POLICER,
            policer_id,
            attr_count,
            attr_list,
            &vs_generic_create);
}

sai_status_t vs_remove_policer(
        _In_ sai_object_id_t policer_id)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_remove_oid(
            SAI_OBJECT_TYPE_POLICER,
            policer_id,
            &vs_generic_remove);
}

sai_status_t vs_set_policer_attribute(
        _In_ sai_object_id_t policer_id,
        _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_set_oid(
            SAI_OBJECT_TYPE_POLICER,
            policer_id,
            attr,
            &vs_generic_set);
}

sai_status_t vs_get_policer_attribute(
        _In_ sai_object_id_t policer_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_get_oid(
            SAI_OBJECT_TYPE_POLICER,
            policer_id,
            attr_count,
            attr_list,
            &vs_generic_get);
}

sai_status_t vs_get_policer_stats(
        _In_ sai_object_id_t policer_id,
        _In_ const sai_policer_stat_counter_t *counter_ids,
        _In_ uint32_t number_of_counters,
        _Out_ uint64_t* counters)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

const sai_policer_api_t vs_policer_api = {
    vs_create_policer,
    vs_remove_policer,
    vs_set_policer_attribute,
    vs_get_policer_attribute,
    vs_get_policer_stats,
};
