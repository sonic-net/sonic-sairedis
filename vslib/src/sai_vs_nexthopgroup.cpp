#include "sai_vs.h"
#include "sai_vs_internal.h"

sai_status_t vs_bulk_object_create_next_hop_group_members(
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

sai_status_t vs_bulk_object_remove_next_hop_group_members(
        _In_ uint32_t object_count,
        _In_ const sai_object_id_t *object_id,
        _In_ sai_bulk_op_type_t type,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

VS_GENERIC_QUAD(NEXT_HOP_GROUP,next_hop_group);
VS_GENERIC_QUAD(NEXT_HOP_GROUP_MEMBER,next_hop_group_member);

const sai_next_hop_group_api_t vs_next_hop_group_api = {

    VS_GENERIC_QUAD_API(next_hop_group)
    VS_GENERIC_QUAD_API(next_hop_group_member)

    vs_bulk_object_create_next_hop_group_members,
    vs_bulk_object_remove_next_hop_group_members,
};
