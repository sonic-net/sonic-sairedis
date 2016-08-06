#include "sai_vs.h"

sai_status_t internal_vs_generic_remove(
        _In_ sai_object_type_t object_type,
        _In_ const std::string &serialized_object_id)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

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

/**
 *  Routine Description:
 *    @brief Removes specified object
 *
 *  Arguments:
 *    @param[in] object_type - the object type
 *    @param[in] object_id - the object id
 *
 *  Return Values:
 *    @return  SAI_STATUS_SUCCESS on success
 *             Failure status code on error
 */
sai_status_t vs_generic_remove(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id)
{
    SWSS_LOG_ENTER();

    std::string str_object_id;
    sai_serialize_primitive(object_id, str_object_id);

    sai_status_t status = internal_vs_generic_remove(
                            object_type,
                            str_object_id);

    return status;
}

sai_status_t vs_generic_remove(
        _In_ sai_object_type_t object_type,
        _In_ const sai_fdb_entry_t* fdb_entry)
{
    SWSS_LOG_ENTER();

    std::string str_fdb_entry;
    sai_serialize_primitive(*fdb_entry, str_fdb_entry);

    sai_status_t status = internal_vs_generic_remove(
                            object_type,
                            str_fdb_entry);

    return status;
}

sai_status_t vs_generic_remove(
        _In_ sai_object_type_t object_type,
        _In_ const sai_neighbor_entry_t* neighbor_entry)
{
    SWSS_LOG_ENTER();

    // rif_id must be valid real id
    std::string str_neighbor_entry;
    sai_serialize_neighbor_entry(*neighbor_entry, str_neighbor_entry);

    sai_status_t status = internal_vs_generic_remove(
                            object_type,
                            str_neighbor_entry);

    return status;
}

sai_status_t vs_generic_remove(
        _In_ sai_object_type_t object_type,
        _In_ const sai_unicast_route_entry_t* unicast_route_entry)
{
    SWSS_LOG_ENTER();

    // vr_id must be valid read virtual router id
    std::string str_route_entry;
    sai_serialize_route_entry(*unicast_route_entry, str_route_entry);

    sai_status_t status = internal_vs_generic_remove(
                            object_type,
                            str_route_entry);

    return status;
}

sai_status_t vs_generic_remove_vlan(
        _In_ sai_object_type_t object_type,
        _In_ sai_vlan_id_t vlan_id)
{
    SWSS_LOG_ENTER();

    std::string str_vlan_id;
    sai_serialize_primitive(vlan_id, str_vlan_id);

    sai_status_t status = internal_vs_generic_remove(
                            object_type,
                            str_vlan_id);

    return status;
}
