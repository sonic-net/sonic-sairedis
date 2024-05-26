#include "sai_stub.h"

static sai_status_t stub_recv_hostif_packet(
        _In_ sai_object_id_t hostif_id,
        _Inout_ sai_size_t *buffer_size,
        _Out_ void *buffer,
        _Inout_ uint32_t *attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

static sai_status_t stub_send_hostif_packet(
        _In_ sai_object_id_t hostif_id,
        _In_ sai_size_t buffer_size,
        _In_ const void *buffer,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

static sai_status_t stub_allocate_hostif_packet(
        _In_ sai_object_id_t hostif_id,
        _In_ sai_size_t buffer_size,
        _Out_ void **buffer,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

static sai_status_t stub_free_hostif_packet(
        _In_ sai_object_id_t hostif_id,
        _Inout_ void *buffer)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

STUB_GENERIC_QUAD(HOSTIF,hostif);
STUB_GENERIC_QUAD(HOSTIF_TABLE_ENTRY,hostif_table_entry);
STUB_GENERIC_QUAD(HOSTIF_TRAP_GROUP,hostif_trap_group);
STUB_GENERIC_QUAD(HOSTIF_TRAP,hostif_trap);
STUB_GENERIC_QUAD(HOSTIF_USER_DEFINED_TRAP,hostif_user_defined_trap);

const sai_hostif_api_t stub_hostif_api = {

    STUB_GENERIC_QUAD_API(hostif)
    STUB_GENERIC_QUAD_API(hostif_table_entry)
    STUB_GENERIC_QUAD_API(hostif_trap_group)
    STUB_GENERIC_QUAD_API(hostif_trap)
    STUB_GENERIC_QUAD_API(hostif_user_defined_trap)

    stub_recv_hostif_packet,
    stub_send_hostif_packet,
    stub_allocate_hostif_packet,
    stub_free_hostif_packet,
};
