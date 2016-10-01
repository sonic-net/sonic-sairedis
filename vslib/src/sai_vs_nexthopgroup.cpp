#include "sai_vs.h"

sai_status_t vs_create_next_hop_group(
        _Out_ sai_object_id_t* next_hop_group_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_create_oid(
            SAI_OBJECT_TYPE_NEXT_HOP_GROUP,
            next_hop_group_id,
            attr_count,
            attr_list,
            &vs_generic_create);
}

sai_status_t vs_remove_next_hop_group(
        _In_ sai_object_id_t next_hop_group_id)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_remove_oid(
            SAI_OBJECT_TYPE_NEXT_HOP_GROUP,
            next_hop_group_id,
            &vs_generic_remove);
}

sai_status_t vs_set_next_hop_group_attribute(
        _In_ sai_object_id_t next_hop_group_id,
        _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_set_oid(
            SAI_OBJECT_TYPE_NEXT_HOP_GROUP,
            next_hop_group_id,
            attr,
            &vs_generic_set);
}

sai_status_t vs_get_next_hop_group_attribute(
        _In_ sai_object_id_t next_hop_group_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_get_oid(
            SAI_OBJECT_TYPE_NEXT_HOP_GROUP,
            next_hop_group_id,
            attr_count,
            attr_list,
            &vs_generic_get);
}

sai_status_t vs_add_next_hop_to_group(
        _In_ sai_object_id_t next_hop_group_id,
        _In_ uint32_t next_hop_count,
        _In_ const sai_object_id_t* nexthops)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t vs_remove_next_hop_from_group(
        _In_ sai_object_id_t next_hop_group_id,
        _In_ uint32_t next_hop_count,
        _In_ const sai_object_id_t* nexthops)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

const sai_next_hop_group_api_t vs_next_hop_group_api = {
    vs_create_next_hop_group,
    vs_remove_next_hop_group,
    vs_set_next_hop_group_attribute,
    vs_get_next_hop_group_attribute,
    vs_add_next_hop_to_group,
    vs_remove_next_hop_from_group,
};
