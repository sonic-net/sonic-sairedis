#include "sai_vs.h"

sai_status_t vs_recv_packet(
        _In_ sai_object_id_t hif_id,
        _Out_ void *buffer,
        _Inout_ sai_size_t *buffer_size,
        _Inout_ uint32_t *attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t vs_send_packet(
        _In_ sai_object_id_t hif_id,
        _In_ void *buffer,
        _In_ sai_size_t buffer_size,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t vs_set_user_defined_trap_attribute(
        _In_ sai_hostif_user_defined_trap_id_t hostif_user_defined_trapid,
        _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t vs_get_user_defined_trap_attribute(
        _In_ sai_hostif_user_defined_trap_id_t hostif_user_defined_trapid,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t vs_create_hostif_trap_group(
        _Out_ sai_object_id_t *hostif_trap_group_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_create_oid(
            SAI_OBJECT_TYPE_TRAP_GROUP,
            hostif_trap_group_id,
            attr_count,
            attr_list,
            &vs_generic_create);
}

sai_status_t vs_remove_hostif_trap_group(
        _In_ sai_object_id_t hostif_trap_group_id)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_remove_oid(
            SAI_OBJECT_TYPE_TRAP_GROUP,
            hostif_trap_group_id,
            &vs_generic_remove);
}

sai_status_t vs_set_trap_group_attribute(
        _In_ sai_object_id_t hostif_trap_group_id,
        _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_set_oid(
            SAI_OBJECT_TYPE_TRAP_GROUP,
            hostif_trap_group_id,
            attr,
            &vs_generic_set);
}

sai_status_t vs_get_trap_group_attribute(
        _In_ sai_object_id_t hostif_trap_group_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_get_oid(
            SAI_OBJECT_TYPE_TRAP_GROUP,
            hostif_trap_group_id,
            attr_count,
            attr_list,
            &vs_generic_get);
}

sai_status_t vs_set_trap_attribute(
        _In_ sai_hostif_trap_id_t hostif_trapid,
        _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_set_trap(
            hostif_trapid,
            attr,
            &vs_generic_set_trap);
}

sai_status_t vs_get_trap_attribute(
        _In_ sai_hostif_trap_id_t hostif_trapid,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_get_trap(
            hostif_trapid,
            attr_count,
            attr_list,
            &vs_generic_get_trap);
}

sai_status_t vs_create_hostif(
        _Out_ sai_object_id_t * hif_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_create_oid(
            SAI_OBJECT_TYPE_HOST_INTERFACE,
            hif_id,
            attr_count,
            attr_list,
            &vs_generic_create);
}

sai_status_t vs_remove_hostif(
        _In_ sai_object_id_t hif_id)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_remove_oid(
            SAI_OBJECT_TYPE_HOST_INTERFACE,
            hif_id,
            &vs_generic_remove);
}

sai_status_t vs_set_hostif_attribute(
        _In_ sai_object_id_t hif_id,
        _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_set_oid(
            SAI_OBJECT_TYPE_HOST_INTERFACE,
            hif_id,
            attr,
            &vs_generic_set);
}

sai_status_t vs_get_hostif_attribute(
        _In_ sai_object_id_t hif_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_get_oid(
            SAI_OBJECT_TYPE_HOST_INTERFACE,
            hif_id,
            attr_count,
            attr_list,
            &vs_generic_get);
}

const sai_hostif_api_t vs_host_interface_api = {
    vs_create_hostif,
    vs_remove_hostif,
    vs_set_hostif_attribute,
    vs_get_hostif_attribute,

    vs_create_hostif_trap_group,
    vs_remove_hostif_trap_group,
    vs_set_trap_group_attribute,
    vs_get_trap_group_attribute,

    vs_set_trap_attribute,
    vs_get_trap_attribute,

    vs_set_user_defined_trap_attribute,
    vs_get_user_defined_trap_attribute,

    vs_recv_packet,
    vs_send_packet,
};
