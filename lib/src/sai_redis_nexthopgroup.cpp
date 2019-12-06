#include "sai_redis.h"

sai_status_t redis_bulk_create_next_hop_group_members(
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

    return redis_bulk_generic_create(
            SAI_OBJECT_TYPE_NEXT_HOP_GROUP_MEMBER,
            object_count,
            object_id,
            switch_id,
            attr_count,
            attrs,
            object_statuses);
}

sai_status_t redis_bulk_remove_next_hop_group_members(
        _In_ uint32_t object_count,
        _In_ const sai_object_id_t *object_id,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return redis_bulk_generic_remove(
            SAI_OBJECT_TYPE_NEXT_HOP_GROUP_MEMBER,
            object_count,
            object_id,
            object_statuses);
}

REDIS_GENERIC_QUAD(NEXT_HOP_GROUP,next_hop_group);
REDIS_GENERIC_QUAD(NEXT_HOP_GROUP_MEMBER,next_hop_group_member);

const sai_next_hop_group_api_t redis_next_hop_group_api = {

    REDIS_GENERIC_QUAD_API(next_hop_group)
    REDIS_GENERIC_QUAD_API(next_hop_group_member)

    redis_bulk_create_next_hop_group_members,
    redis_bulk_remove_next_hop_group_members,
};
