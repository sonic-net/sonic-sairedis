#include "sai_vs.h"
#include "sai_vs_internal.h"

sai_status_t vs_create_segmentroute_sidlists(
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t object_count,
        _In_ const uint32_t *attr_count,
        _In_ const sai_attribute_t **attrs,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_object_id_t *object_id,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t vs_remove_segmentroute_sidlists(
        _In_ uint32_t object_count,
        _In_ const sai_object_id_t *object_id,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

VS_GENERIC_QUAD(SEGMENTROUTE_SIDLIST,segmentroute_sidlist);

const sai_segmentroute_api_t vs_segmentroute_api = {

    VS_GENERIC_QUAD_API(segmentroute_sidlist)

    vs_create_segmentroute_sidlists,
    vs_remove_segmentroute_sidlists,
};
