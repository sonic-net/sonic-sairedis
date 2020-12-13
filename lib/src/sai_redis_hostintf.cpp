#include "sai_redis.h"

static sai_status_t redis_recv_hostif_packet(
        _In_ sai_object_id_t hostif_id,
        _Inout_ sai_size_t *buffer_size,
        _Out_ void *buffer,
        _Inout_ uint32_t *attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

static sai_status_t redis_send_hostif_packet(
        _In_ sai_object_id_t hostif_id,
        _In_ sai_size_t buffer_size,
        _In_ const void *buffer,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

static sai_status_t redis_allocate_hostif_packet(
        _In_ sai_object_id_t hostif_id,
        _In_ sai_size_t buffer_size,
        _Out_ void **buffer,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

static sai_status_t redis_free_hostif_packet(
        _In_ sai_object_id_t hostif_id,
        _Inout_ void *buffer)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

REDIS_GENERIC_QUAD(HOSTIF,hostif);
REDIS_GENERIC_QUAD(HOSTIF_TABLE_ENTRY,hostif_table_entry);
REDIS_GENERIC_QUAD(HOSTIF_TRAP_GROUP,hostif_trap_group);
REDIS_GENERIC_QUAD(HOSTIF_TRAP,hostif_trap);
REDIS_GENERIC_QUAD(HOSTIF_USER_DEFINED_TRAP,hostif_user_defined_trap);

const sai_hostif_api_t redis_hostif_api = {

    REDIS_GENERIC_QUAD_API(hostif)
    REDIS_GENERIC_QUAD_API(hostif_table_entry)
    REDIS_GENERIC_QUAD_API(hostif_trap_group)
    REDIS_GENERIC_QUAD_API(hostif_trap)
    REDIS_GENERIC_QUAD_API(hostif_user_defined_trap)

    redis_recv_hostif_packet,
    redis_send_hostif_packet,
    redis_allocate_hostif_packet,
    redis_free_hostif_packet,
};
