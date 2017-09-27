#include "sai_redis.h"

sai_status_t redis_create_stp_ports(
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t object_count,
        _In_ const uint32_t *attr_count,
        _In_ const sai_attribute_t **attrs,
        _In_ sai_bulk_op_type_t type,
        _Out_ sai_object_id_t *object_id,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t redis_remove_stp_ports(
        _In_ uint32_t object_count,
        _In_ const sai_object_id_t *object_id,
        _In_ sai_bulk_op_type_t type,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

REDIS_GENERIC_QUAD(STP,stp);
REDIS_GENERIC_QUAD(STP_PORT,stp_port);


const sai_stp_api_t redis_stp_api = {

    REDIS_GENERIC_QUAD_API(stp)
    REDIS_GENERIC_QUAD_API(stp_port)

    redis_create_stp_ports,
    redis_remove_stp_ports,
};
