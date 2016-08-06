#include "sai_vs.h"

ObjectHash g_objectHash;

uint64_t g_real_id = 0;

sai_object_id_t vs_create_real_object_id(
        _In_ sai_object_type_t object_type)
{
    SWSS_LOG_ENTER();

    g_real_id++;

    sai_object_id_t objectId = (((sai_object_id_t)object_type) << 48) | g_real_id;

    SWSS_LOG_DEBUG("created RID %llx", objectId);

    return objectId;
}

/**
 * Routine Description:
 *     @brief  Query sai object type.
 *
 * Arguments:
 *     @param[in] sai_object_id
 *
 * Return Values:
 *    @return  Return SAI_OBJECT_TYPE_NULL when sai_object_id is not valid.
 *             Otherwise, return a valid sai object type SAI_OBJECT_TYPE_XXX
 */
sai_object_type_t sai_object_type_query(_In_ sai_object_id_t sai_object_id)
{
    return (sai_object_type_t)(sai_object_id >> 48);
}

/**
 *   Routine Description:
 *    @brief Generic create method
 *
 *  Arguments:
 *  @param[out] acl_table_id - the the acl table id
 *  @param[in] attr_count - number of attributes
 *  @param[in] attr_list - array of attributes
 *
 *  Return Values:
 *    @return  SAI_STATUS_SUCCESS on success
 *             Failure status code on error
 */
sai_status_t internal_vs_generic_create(
        _In_ sai_object_type_t object_type,
        _In_ const std::string &serialized_object_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    std::lock_guard<std::recursive_mutex> lock(g_recursive_mutex);

    SWSS_LOG_ENTER();

    auto it = g_objectHash.find(serialized_object_id);

    if (it != g_objectHash.end())
    {
        SWSS_LOG_ERROR("Create failed, object already exists, object type: %d: id: %s", object_type, serialized_object_id.c_str());

        return SAI_STATUS_ITEM_ALREADY_EXISTS;
    }

    std::vector<swss::FieldValueTuple> values = SaiAttributeList::serialize_attr_list(
            object_type,
            attr_count,
            attr_list,
            false);

    // we can have multiple attributes when creating object, we need to set each one
    // in hash per object, since get can request multiple attributes

    AttrHash attrHash;

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        const std::string &str_attr_id = fvField(values[i]);
        const std::string &str_attr_value = fvValue(values[i]);

        attrHash[str_attr_id] = str_attr_value;
    }

    g_objectHash[serialized_object_id] = attrHash; // set attributes to object id

    SWSS_LOG_DEBUG("Create succeeded, object type: %d, id: %s", object_type, serialized_object_id.c_str());

    return SAI_STATUS_SUCCESS;
}

/**
 *   Routine Description:
 *    @brief Generic create method
 *
 *  Arguments:
 *  @param[out] acl_table_id - the the acl table id
 *  @param[in] attr_count - number of attributes
 *  @param[in] attr_list - array of attributes
 *
 *  Return Values:
 *    @return  SAI_STATUS_SUCCESS on success
 *             Failure status code on error
 */
sai_status_t vs_generic_create(
        _In_ sai_object_type_t object_type,
        _Out_ sai_object_id_t* object_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    // create new real object ID
    *object_id = vs_create_real_object_id(object_type);

    std::string str_object_id;
    sai_serialize_primitive(*object_id, str_object_id);

    sai_status_t status = internal_vs_generic_create(
            object_type,
            str_object_id,
            attr_count,
            attr_list);

    return status;
}

sai_status_t vs_generic_create(
        _In_ sai_object_type_t object_type,
        _In_ const sai_fdb_entry_t *fdb_entry,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    std::string str_fdb_entry;
    sai_serialize_primitive(*fdb_entry, str_fdb_entry);

    sai_status_t status = internal_vs_generic_create(
            object_type,
            str_fdb_entry,
            attr_count,
            attr_list);

    return status;
}

sai_status_t vs_generic_create(
        _In_ sai_object_type_t object_type,
        _In_ const sai_neighbor_entry_t* neighbor_entry,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    // rif_id must be valid real id
    std::string str_neighbor_entry;
    sai_serialize_neighbor_entry(*neighbor_entry, str_neighbor_entry);

    sai_status_t status = internal_vs_generic_create(
            object_type,
            str_neighbor_entry,
            attr_count,
            attr_list);

    return status;
}

sai_status_t vs_generic_create(
        _In_ sai_object_type_t object_type,
        _In_ const sai_unicast_route_entry_t* unicast_route_entry,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    // vr_id must be valid real virtual router id
    std::string str_route_entry;
    sai_serialize_route_entry(*unicast_route_entry, str_route_entry);

    sai_status_t status = internal_vs_generic_create(
            object_type,
            str_route_entry,
            attr_count,
            attr_list);

    return status;
}

sai_status_t vs_generic_create_vlan(
        _In_ sai_object_type_t object_type,
        _In_ sai_vlan_id_t vlan_id)
{
    SWSS_LOG_ENTER();

    std::string str_vlan_id;
    sai_serialize_primitive(vlan_id, str_vlan_id);

    // create_vlan have no attributes, but generic method needs one
    // so provide count as zero and dummy attribute
    sai_attribute_t dummy_attribute;

    sai_status_t status = internal_vs_generic_create(
            object_type,
            str_vlan_id,
            0,
            &dummy_attribute);

    return status;
}
