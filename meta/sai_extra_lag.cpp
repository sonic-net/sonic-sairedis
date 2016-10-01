#include "sai_meta.h"
#include "sai_extra.h"

sai_status_t meta_pre_create_lag(
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_remove_lag(
        _In_ sai_object_id_t lag_id)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_set_lag_attribute(
        _In_ sai_object_id_t lag_id,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_get_lag_attribute(
        _In_ sai_object_id_t lag_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_create_lag_member(
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    // TODO check if this port is not already a member in this LAG, can it be duplicated?

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_remove_lag_member(
        _In_ sai_object_id_t lag_member_id)
{
    SWSS_LOG_ENTER();

    // since lag member is leaf it should be always safe to remove lag member
    // but can there exist lag without members ?

    return SAI_STATUS_SUCCESS;
}


sai_status_t meta_pre_set_lag_member_attribute(
        _In_ sai_object_id_t lag_member_id,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_get_lag_member_attribute(
        _In_ sai_object_id_t lag_member_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}
