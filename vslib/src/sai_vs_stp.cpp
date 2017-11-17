#include "sai_vs.h"
#include "sai_vs_internal.h"

sai_status_t vs_create_stp_ports(
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

sai_status_t vs_remove_stp_ports(
        _In_ uint32_t object_count,
        _In_ const sai_object_id_t *object_id,
        _In_ sai_bulk_op_type_t type,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

VS_GENERIC_QUAD(STP,stp);
VS_GENERIC_QUAD(STP_PORT,stp_port);


const sai_stp_api_t vs_stp_api = {

    VS_GENERIC_QUAD_API(stp)
    VS_GENERIC_QUAD_API(stp_port)

    vs_create_stp_ports,
    vs_remove_stp_ports,
};
