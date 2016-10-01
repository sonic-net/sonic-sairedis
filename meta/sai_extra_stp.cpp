#include "sai_meta.h"
#include "sai_extra.h"

sai_status_t meta_pre_create_stp(
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_remove_stp(
        _In_ sai_object_id_t stp_id)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_set_stp_attribute(
        _In_ sai_object_id_t stp_id,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_get_stp_attribute(
        _In_ sai_object_id_t stp_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}
