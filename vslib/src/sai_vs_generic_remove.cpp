#include "sai_vs.h"
#include "sai_vs_state.h"

sai_status_t internal_vs_generic_remove(
        _In_ sai_object_type_t object_type,
        _In_ const std::string &serialized_object_id)
{
    SWSS_LOG_ENTER();

    auto it = g_objectHash.find(serialized_object_id);

    if (it == g_objectHash.end())
    {
        SWSS_LOG_ERROR("Remove failed, object not found, object type: %d: id: %s", object_type, serialized_object_id.c_str());

        return SAI_STATUS_ITEM_NOT_FOUND;
    }

    g_objectHash.erase(it);

    SWSS_LOG_DEBUG("Remove succeeded, object type: %d, id: %s", object_type, serialized_object_id.c_str());

    return SAI_STATUS_SUCCESS;
}

sai_status_t vs_generic_remove(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id)
{
    SWSS_LOG_ENTER();

    std::string str_object_id = sai_serialize_object_id(object_id);

    return internal_vs_generic_remove(
            object_type,
            str_object_id);
}

sai_status_t vs_generic_remove_fdb_entry(
        _In_ const sai_fdb_entry_t* fdb_entry)
{
    SWSS_LOG_ENTER();

    std::string str_fdb_entry = sai_serialize_fdb_entry(*fdb_entry);

    return internal_vs_generic_remove(
            SAI_OBJECT_TYPE_FDB,
            str_fdb_entry);
}

sai_status_t vs_generic_remove_neighbor_entry(
        _In_ const sai_neighbor_entry_t* neighbor_entry)
{
    SWSS_LOG_ENTER();

    std::string str_neighbor_entry = sai_serialize_neighbor_entry(*neighbor_entry);

    return internal_vs_generic_remove(
            SAI_OBJECT_TYPE_NEIGHBOR,
            str_neighbor_entry);
}

sai_status_t vs_generic_remove_route_entry(
        _In_ const sai_unicast_route_entry_t* unicast_route_entry)
{
    SWSS_LOG_ENTER();

    std::string str_route_entry = sai_serialize_route_entry(*unicast_route_entry);

    return internal_vs_generic_remove(
            SAI_OBJECT_TYPE_ROUTE,
            str_route_entry);
}

sai_status_t vs_generic_remove_vlan(
        _In_ sai_vlan_id_t vlan_id)
{
    SWSS_LOG_ENTER();

    std::string str_vlan_id = sai_serialize_vlan_id(vlan_id);

    return internal_vs_generic_remove(
            SAI_OBJECT_TYPE_VLAN,
            str_vlan_id);
}
