#include "sai_meta.h"
#include "sai_extra.h"

sai_status_t meta_pre_create_tunnel_map(
        _Out_ sai_object_id_t* tunnel_map_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    const sai_attribute_t* attr_map_to_value_list = get_attribute_by_id(SAI_TUNNEL_MAP_ATTR_MAP_TO_VALUE_LIST, attr_count, attr_list);

    // TODO check if additional validation is needed on those types
    // SAI_TUNNEL_MAP_OECN_TO_UECN:
    // SAI_TUNNEL_MAP_UECN_OECN_TO_OECN:
    // SAI_TUNNEL_MAP_VNI_TO_VLAN_ID:
    // SAI_TUNNEL_MAP_VLAN_ID_TO_VNI:

    // TODO validate tunnel map list

    if (attr_map_to_value_list != NULL)
    {
        const sai_tunnel_map_list_t* tunnel_map_list = &attr_map_to_value_list->value.tunnelmap;

        if (tunnel_map_list->list == NULL)
        {
            SWSS_LOG_ERROR("tunnel map list is NULL");

            return SAI_STATUS_INVALID_PARAMETER;
        }

        for (uint32_t i = 0; i < tunnel_map_list->count; ++i)
        {
            const sai_tunnel_map_t* tunnel_map = &tunnel_map_list->list[i];

            // TODO validate tunnel map
            SWSS_LOG_DEBUG("tunnel map pointer: %p", tunnel_map);
        }
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_remove_tunnel_map(
        _In_ sai_object_id_t tunnel_map_id)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_set_tunnel_map_attribute(
        _In_ sai_object_id_t tunnel_map_id,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_get_tunnel_map_attribute(
        _In_ sai_object_id_t   tunnel_map_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_create_tunnel(
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    // TODO for GRE/VXLAN it may be different type (router interface, overlay/underlay
    //
    // TODO validate object on that list! if they exist
    // shoud this list contain at least 1 element ? or can it be empty?
    // check for duplicates on list ? - ecn mappers

    // TODO sai spec is inconsisten here, if this is mandatory attribute on some condition,
    // then it cannot have default value, dscp mode and ttl mode

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_remove_tunnel(
        _In_ sai_object_id_t tunnel_id)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_set_tunnel_attribute(
        _In_ sai_object_id_t tunnel_id,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    switch (attr->id)
    {
        case SAI_TUNNEL_ATTR_ENCAP_ECN_MODE:
        case SAI_TUNNEL_ATTR_ENCAP_MAPPERS:

            // TODO validate this use case

            break;

        case SAI_TUNNEL_ATTR_DECAP_ECN_MODE:
        case SAI_TUNNEL_ATTR_DECAP_MAPPERS:

            // TODO validate this use case

            break;

        default:

            SWSS_LOG_ERROR("set attribute id %d is not allowed", attr->id);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_get_tunnel_attribute(
        _In_ sai_object_id_t tunnel_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_create_tunnel_term_table_entry (
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    // TODO check is this conditional attribute, maybe this action is only
    // required for ip in ip tunnel types

    // TODO additional checks may be required sinec this action tunnel id is used for
    // decap so maybe this tunnel must have special attributes on creation set

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_remove_tunnel_term_table_entry (
        _In_ sai_object_id_t tunnel_term_table_entry_id)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_set_tunnel_term_table_entry_attribute(
        _In_ sai_object_id_t tunnel_term_table_entry_id,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}

sai_status_t meta_pre_get_tunnel_term_table_entry_attribute(
        _In_ sai_object_id_t tunnel_term_table_entry_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return SAI_STATUS_SUCCESS;
}
