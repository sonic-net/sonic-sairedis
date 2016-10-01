#include "sai_meta.h"
#include "sai_extra.h"

sai_status_t meta_pre_create_route(
    _In_ const sai_unicast_route_entry_t* unicast_route_entry,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_remove_route(
    _In_ const sai_unicast_route_entry_t* unicast_route_entry)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_set_route_attribute(
    _In_ const sai_unicast_route_entry_t* unicast_route_entry,
    _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_get_route_attribute(
    _In_ const sai_unicast_route_entry_t* unicast_route_entry,
    _In_ uint32_t attr_count,
    _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}
