#include "sai_meta.h"
#include "sai_extra.h"

sai_status_t meta_pre_create_hostif_trap_group(
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_remove_hostif_trap_group(
        _In_ sai_object_id_t hostif_trap_group_id)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_set_trap_group_attribute(
        _In_ sai_object_id_t hostif_trap_group_id,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    // SAI_HOSTIF_TRAP_GROUP_ATTR_QUEUE:

    // TODO this can be tricky since this QUEUE is queue INDEX
    // indirect depenency, by default there are 8 queues, but
    // user can create extra one, so there may be 10, and what
    // happens when this points to queue 10 and user remove this queue?
    // on queue remove we should queue index on trap group and
    // not allow to remove then

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_get_trap_group_attribute(
        _In_ sai_object_id_t hostif_trap_group_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_set_trap_attribute(
        _In_ sai_hostif_trap_id_t hostif_trapid,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    // case SAI_HOSTIF_TRAP_ATTR_TRAP_CHANNEL:
    // TODO extra validation here maybe needed to check cb/fd/netdev
    // we will need extra logic to validate this attribute (previous attributes)

    // case SAI_HOSTIF_TRAP_ATTR_FD:
    // Valid only when SAI_HOSTIF_TRAP_ATTR_TRAP_CHANNEL == SAI_HOSTIF_TRAP_CHANNEL_FD
    // Must be set before set SAI_HOSTIF_TRAP_ATTR_TRAP_CHANNEL to SAI_HOSTIF_TRAP_CHANNEL_FD

    // TODO additional logic here is required, this fd hostif
    // must be type of sai_hostif_type_t == SAI_HOSTIF_TYPE_FD
    // we need to check hostif attribute then
    //
    // TODO also what about netdev case ?

    // SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP:
    // TODO that default trap group should also be
    // on that trap groups list this would implify condition

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_get_trap_attribute(
        _In_ sai_hostif_trap_id_t hostif_trapid,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_create_hostif(
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    // case SAI_HOSTIF_TYPE_FD:
    // TODO check if it contains only ASCII and whether name is not used
    // by other host interface

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_remove_hostif(
        _In_ sai_object_id_t hif_id)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_set_hostif_attribute(
        _In_ sai_object_id_t hif_id,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_get_hostif_attribute(
        _In_ sai_object_id_t hif_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

