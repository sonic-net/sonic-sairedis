#include "sai_redis.h"

/**
 *   Routine Description:
 *    @brief Internal set attribute
 *
 *  Arguments:
 *  @param[in] object_type - type of object
 *  @param[in] serialized_object_id - serialized object id
 *  @param[in] attr - attribute to serialize
 *
 *  Return Values:
 *    @return  SAI_STATUS_SUCCESS on success
 *             Failure status code on error
 */
sai_status_t internal_redis_generic_set(
        _In_ sai_object_type_t object_type,
        _In_ const std::string &serialized_object_id,
        _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_mutex);

    SWSS_LOG_ENTER();

    std::vector<swss::FieldValueTuple> entry = SaiAttributeList::serialize_attr_list(
            object_type, 
            1,
            attr,
            false);

    std::string str_object_type;

    sai_serialize_primitive(object_type, str_object_type);

    std::string key = str_object_type + ":" + serialized_object_id;

    SWSS_LOG_DEBUG("generic set key: %s, fields: %lu", key.c_str(), entry.size());

    g_asicState->set(key, entry, "set");

    return SAI_STATUS_SUCCESS;
}

sai_status_t redis_generic_set(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    if (object_id == SAI_NULL_OBJECT_ID && object_type != SAI_OBJECT_TYPE_SWITCH)
    {
        SWSS_LOG_ERROR("object id is zero on object type %d", object_type);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    std::string str_object_id;
    sai_serialize_primitive(object_id, str_object_id);

    sai_status_t status = internal_redis_generic_set(
            object_type,
            str_object_id,
            attr);

    return status;
}

sai_status_t redis_generic_set(
        _In_ sai_object_type_t object_type,
        _In_ const sai_fdb_entry_t *fdb_entry,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    std::string str_fdb_entry;
    sai_serialize_primitive(*fdb_entry, str_fdb_entry);

    sai_status_t status = internal_redis_generic_set(
            object_type,
            str_fdb_entry,
            attr);

    return status;
}

sai_status_t redis_generic_set(
        _In_ sai_object_type_t object_type,
        _In_ const sai_neighbor_entry_t* neighbor_entry,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    std::string str_neighbor_entry;
    sai_serialize_neighbor_entry(*neighbor_entry, str_neighbor_entry);

    sai_status_t status = internal_redis_generic_set(
            object_type,
            str_neighbor_entry,
            attr);

    return status;
}

sai_status_t redis_generic_set(
        _In_ sai_object_type_t object_type,
        _In_ const sai_unicast_route_entry_t* unicast_route_entry,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    std::string str_route_entry;
    sai_serialize_route_entry(*unicast_route_entry, str_route_entry);

    sai_status_t status = internal_redis_generic_set(
            object_type,
            str_route_entry,
            attr);

    return status;
}

sai_status_t redis_generic_set_vlan(
        _In_ sai_object_type_t object_type,
        _In_ sai_vlan_id_t vlan_id,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    std::string str_vlan_id;
    sai_serialize_primitive(vlan_id, str_vlan_id);

    sai_status_t status = internal_redis_generic_set(
            object_type,
            str_vlan_id,
            attr);

    return status;
}
