#include "sai_vs.h"
#include "sai_vs_state.h"

sai_status_t internal_vs_get_process(
        _In_ sai_object_type_t object_type,
        _In_ const std::string &str_attr_id,
        _In_ const std::string &str_attr_value,
        _Out_ sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    // TODO we currently don't check get list count
    // so we will not get SAI_STATUS_BUFFER_OVERFLOW
    // count needs to be exact or less then actual attribute count
    // we will need extra transfer method for that or method that
    // can extract count from attribute

    std::vector<swss::FieldValueTuple> values;

    values.push_back(swss::FieldValueTuple(str_attr_id, str_attr_value));

    SaiAttributeList list(object_type, values, false);

    transfer_attributes(object_type, 1, list.get_attr_list(), attr, false);

    return SAI_STATUS_SUCCESS;
}

sai_status_t internal_vs_generic_get(
        _In_ sai_object_type_t object_type,
        _In_ const std::string &serialized_object_id,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    auto it = g_objectHash.find(serialized_object_id);

    if (it == g_objectHash.end())
    {
        SWSS_LOG_ERROR("Get failed, object not found, object type: %d: id: %s", object_type, serialized_object_id.c_str());

        return SAI_STATUS_ITEM_NOT_FOUND;
    }

    std::vector<swss::FieldValueTuple> values = SaiAttributeList::serialize_attr_list(
            object_type,
            attr_count,
            attr_list,
            false);

    AttrHash attrHash = it->second;

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        const std::string &str_attr_id = fvField(values[i]);

        // actual count that user requests is in attr_value
        // if requested item is a list, then it must be extracted
        // const std::string &str_attr_value = fvValue(values[i]);

        auto ait = attrHash.find(str_attr_id);

        if (ait == attrHash.end())
        {
            SWSS_LOG_ERROR("Get failed, attribute not found, object type: %d: id: %s, attr_id: %s",
                    object_type,
                    serialized_object_id.c_str(),
                    str_attr_id.c_str());

            return SAI_STATUS_ITEM_NOT_FOUND;
        }

        const std::string &str_attr_value = ait->second;

        sai_status_t status = internal_vs_get_process(
                object_type,
                str_attr_id,
                str_attr_value,
                &attr_list[i]);

        if (status != SAI_STATUS_SUCCESS)
        {

            SWSS_LOG_ERROR("Get failed, attribute process failed, object type: %d: id: %s, attr_id: %s",
                    object_type,
                    serialized_object_id.c_str(),
                    str_attr_id.c_str());

            return SAI_STATUS_FAILURE;
        }
    }

    SWSS_LOG_DEBUG("Get succeeded, object type: %d, id: %s", object_type, serialized_object_id.c_str());

    return SAI_STATUS_SUCCESS;
}

sai_status_t vs_generic_get(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    if (object_id == SAI_NULL_OBJECT_ID && object_type != SAI_OBJECT_TYPE_SWITCH)
    {
        SWSS_LOG_ERROR("object id is zero on object type %d", object_type);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    std::string str_object_id;
    sai_serialize_primitive(object_id, str_object_id);

    sai_status_t status = internal_vs_generic_get(
            object_type,
            str_object_id,
            attr_count,
            attr_list);

    return status;
}

sai_status_t vs_generic_get_fdb_entry(
        _In_ const sai_fdb_entry_t *fdb_entry,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    std::string str_fdb_entry;
    sai_serialize_primitive(*fdb_entry, str_fdb_entry);

    sai_status_t status = internal_vs_generic_get(
            SAI_OBJECT_TYPE_FDB,
            str_fdb_entry,
            attr_count,
            attr_list);

    return status;
}

sai_status_t vs_generic_get_neighbor_entry(
        _In_ const sai_neighbor_entry_t* neighbor_entry,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    std::string str_neighbor_entry;
    sai_serialize_neighbor_entry(*neighbor_entry, str_neighbor_entry);

    sai_status_t status = internal_vs_generic_get(
            SAI_OBJECT_TYPE_NEIGHBOR,
            str_neighbor_entry,
            attr_count,
            attr_list);

    return status;
}

sai_status_t vs_generic_get_route_entry(
        _In_ const sai_unicast_route_entry_t* unicast_route_entry,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    std::string str_route_entry;
    sai_serialize_route_entry(*unicast_route_entry, str_route_entry);

    sai_status_t status = internal_vs_generic_get(
            SAI_OBJECT_TYPE_ROUTE,
            str_route_entry,
            attr_count,
            attr_list);

    return status;
}

sai_status_t vs_generic_get_vlan(
        _In_ sai_vlan_id_t vlan_id,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    std::string str_vlan_id;
    sai_serialize_primitive(vlan_id, str_vlan_id);

    sai_status_t status = internal_vs_generic_get(
            SAI_OBJECT_TYPE_VLAN,
            str_vlan_id,
            attr_count,
            attr_list);

    return status;
}

sai_status_t vs_generic_get_trap(
        _In_ sai_hostif_trap_id_t hostif_trapid,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    return vs_generic_get(
            SAI_OBJECT_TYPE_TRAP,
            hostif_trapid,
            attr_count,
            attr_list);
}

sai_status_t vs_generic_get_switch(
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_object_id_t object_id = switch_object_id;

    std::string str_object_id;
    sai_serialize_primitive(object_id, str_object_id);

    sai_status_t status = internal_vs_generic_get(
            SAI_OBJECT_TYPE_SWITCH,
            str_object_id,
            attr_count,
            attr_list);

    return status;
}

