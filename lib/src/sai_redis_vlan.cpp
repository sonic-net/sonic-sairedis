#include "sai_redis.h"

sai_status_t redis_create_vlan_members(
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

sai_status_t redis_remove_vlan_members(
        _In_ uint32_t object_count,
        _In_ const sai_object_id_t *object_id,
        _In_ sai_bulk_op_type_t type,
        _Out_ sai_status_t *object_statuses)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t redis_get_vlan_stats(
        _In_ sai_object_id_t vlan_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_vlan_stat_t *counter_ids,
        _Out_ uint64_t *counters)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t redis_clear_vlan_stats(
        _In_ sai_object_id_t vlan_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_vlan_stat_t *counter_ids)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

REDIS_GENERIC_QUAD(VLAN,vlan);
REDIS_GENERIC_QUAD(VLAN_MEMBER,vlan_member);

const sai_vlan_api_t redis_vlan_api = {

    REDIS_GENERIC_QUAD_API(vlan)
    REDIS_GENERIC_QUAD_API(vlan_member)

    redis_create_vlan_members,
    redis_remove_vlan_members,
    redis_get_vlan_stats,
    redis_clear_vlan_stats,
};
