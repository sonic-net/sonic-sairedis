#include "sai_meta.h"
#include "sai_extra.h"

sai_status_t meta_pre_create_neighbor_entry(
    _In_ const sai_neighbor_entry_t* neighbor_entry,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_remove_neighbor_entry(
    _In_ const sai_neighbor_entry_t* neighbor_entry)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_set_neighbor_attribute(
    _In_ const sai_neighbor_entry_t* neighbor_entry,
    _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_get_neighbor_attribute(
    _In_ const sai_neighbor_entry_t* neighbor_entry,
    _In_ uint32_t attr_count,
    _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}
