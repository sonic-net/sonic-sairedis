#include "sai_proxy.h"

static sai_status_t proxy_recv_hostif_packet(
        _In_ sai_object_id_t hostif_id,
        _Inout_ sai_size_t *buffer_size,
        _Out_ void *buffer,
        _Inout_ uint32_t *attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

static sai_status_t proxy_send_hostif_packet(
        _In_ sai_object_id_t hostif_id,
        _In_ sai_size_t buffer_size,
        _In_ const void *buffer,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

static sai_status_t proxy_allocate_hostif_packet(
        _In_ sai_object_id_t hostif_id,
        _In_ sai_size_t buffer_size,
        _Out_ void **buffer,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

static sai_status_t proxy_free_hostif_packet(
        _In_ sai_object_id_t hostif_id,
        _Inout_ void *buffer)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

PROXY_GENERIC_QUAD(HOSTIF,hostif);
PROXY_GENERIC_QUAD(HOSTIF_TABLE_ENTRY,hostif_table_entry);
PROXY_GENERIC_QUAD(HOSTIF_TRAP_GROUP,hostif_trap_group);
PROXY_GENERIC_QUAD(HOSTIF_TRAP,hostif_trap);
PROXY_GENERIC_QUAD(HOSTIF_USER_DEFINED_TRAP,hostif_user_defined_trap);

const sai_hostif_api_t proxy_hostif_api = {

    PROXY_GENERIC_QUAD_API(hostif)
    PROXY_GENERIC_QUAD_API(hostif_table_entry)
    PROXY_GENERIC_QUAD_API(hostif_trap_group)
    PROXY_GENERIC_QUAD_API(hostif_trap)
    PROXY_GENERIC_QUAD_API(hostif_user_defined_trap)

    proxy_recv_hostif_packet,
    proxy_send_hostif_packet,
    proxy_allocate_hostif_packet,
    proxy_free_hostif_packet,
};
