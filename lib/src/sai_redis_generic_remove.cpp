#include "sai_redis.h"

sai_status_t internal_redis_generic_remove(
        _In_ sai_object_type_t object_type,
        _In_ const std::string &serialized_object_id)
{
    std::lock_guard<std::mutex> lock(g_mutex);

    SWSS_LOG_ENTER();

    std::string str_object_type;

    sai_serialize_primitive(object_type, str_object_type);

    std::string key = str_object_type + ":" + serialized_object_id;

    SWSS_LOG_DEBUG("generic remove key: %s", key.c_str());

    g_asicState->del(key, "remove");

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
sai_status_t redis_generic_remove(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id)
{
    SWSS_LOG_ENTER();

    if (object_id == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_ERROR("object id is zero on object type %d", object_type);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    std::string str_object_id;
    sai_serialize_primitive(object_id, str_object_id);

    sai_status_t status = internal_redis_generic_remove(
                            object_type,
                            str_object_id);

    return status;
}

sai_status_t redis_generic_remove(
        _In_ sai_object_type_t object_type,
        _In_ const sai_fdb_entry_t* fdb_entry)
{
    SWSS_LOG_ENTER();

    std::string str_fdb_entry;
    sai_serialize_primitive(*fdb_entry, str_fdb_entry);

    sai_status_t status = internal_redis_generic_remove(
                            object_type,
                            str_fdb_entry);

    return status;
}

sai_status_t redis_generic_remove(
        _In_ sai_object_type_t object_type,
        _In_ const sai_neighbor_entry_t* neighbor_entry)
{
    SWSS_LOG_ENTER();

    // rif_id must be valid virtual id
    std::string str_neighbor_entry;
    sai_serialize_neighbor_entry(*neighbor_entry, str_neighbor_entry);

    sai_status_t status = internal_redis_generic_remove(
                            object_type,
                            str_neighbor_entry);

    return status;
}

sai_status_t redis_generic_remove(
        _In_ sai_object_type_t object_type,
        _In_ const sai_unicast_route_entry_t* unicast_route_entry)
{
    SWSS_LOG_ENTER();

    // vr_id must be valid virtual router id
    std::string str_route_entry;
    sai_serialize_route_entry(*unicast_route_entry, str_route_entry);

    sai_status_t status = internal_redis_generic_remove(
                            object_type,
                            str_route_entry);

    return status;
}

sai_status_t redis_generic_remove_vlan(
        _In_ sai_object_type_t object_type,
        _In_ sai_vlan_id_t vlan_id)
{
    SWSS_LOG_ENTER();

    std::string str_vlan_id;
    sai_serialize_primitive(vlan_id, str_vlan_id);

    sai_status_t status = internal_redis_generic_remove(
                            object_type,
                            str_vlan_id);

    return status;
}
