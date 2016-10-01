#include "sai_vs.h"

sai_status_t vs_create_stp(
        _Out_ sai_object_id_t *stp_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_create_oid(
            SAI_OBJECT_TYPE_STP_INSTANCE,
            stp_id,
            attr_count,
            attr_list,
            &vs_generic_create);
}

sai_status_t vs_remove_stp(
        _In_ sai_object_id_t stp_id)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_remove_oid(
            SAI_OBJECT_TYPE_STP_INSTANCE,
            stp_id,
            &vs_generic_remove);
}

sai_status_t vs_set_stp_attribute(
        _In_ sai_object_id_t stp_id,
        _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_set_oid(
            SAI_OBJECT_TYPE_STP_INSTANCE,
            stp_id,
            attr,
            &vs_generic_set);
}

sai_status_t vs_get_stp_attribute(
        _In_ sai_object_id_t stp_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    return meta_sai_get_oid(
            SAI_OBJECT_TYPE_STP_INSTANCE,
            stp_id,
            attr_count,
            attr_list,
            &vs_generic_get);
}

sai_status_t vs_set_stp_port_state(
        _In_ sai_object_id_t stp_id,
        _In_ sai_object_id_t port_id,
        _In_ sai_port_stp_port_state_t stp_port_state)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t vs_get_stp_port_state(
        _In_ sai_object_id_t stp_id,
        _In_ sai_object_id_t port_id,
        _Out_ sai_port_stp_port_state_t *stp_port_state)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

const sai_stp_api_t vs_stp_api = {
    vs_create_stp,
    vs_remove_stp,
    vs_set_stp_attribute,
    vs_get_stp_attribute,
    vs_set_stp_port_state,
    vs_get_stp_port_state,
};
