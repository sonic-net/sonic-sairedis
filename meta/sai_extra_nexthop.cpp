#include "sai_meta.h"
#include "sai_extra.h"

sai_status_t meta_pre_create_next_hop(
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    // SAI_NEXT_HOP_TUNNEL_ENCAP:
    // TODO tunnel is existing, should be make some additional checks ? like tunnel encap type?

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_remove_next_hop(
        _In_ sai_object_id_t next_hop_id)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_set_next_hop_attribute(
        _In_ sai_object_id_t next_hop_id,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_get_next_hop_attribute(
        _In_ sai_object_id_t next_hop_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}
