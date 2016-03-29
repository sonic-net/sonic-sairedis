#include "sai_redis.h"

sai_status_t internal_redis_get_process(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list,
        _In_ swss::KeyOpFieldsValuesTuple &kco)
{
    REDIS_LOG_ENTER();

    // key is: object_type:object_id:sai_status

    const std::string &key = kfvKey(kco);
    const std::vector<swss::FieldValueTuple> &values = kfvFieldsValues(kco);

    std::string str_sai_status = key;

    sai_status_t status;

    int index = 0;
    sai_deserialize_primitive(str_sai_status, index, status);

    // we could deserialize directly to user data, but list is alocated by deserializer
    if (status == SAI_STATUS_SUCCESS)
    {
        SaiAttributeList list(object_type, values, false);

        transfer_attributes(object_type, attr_count, list.get_attr_list(), attr_list, false);
    }
    else if (status == SAI_STATUS_BUFFER_OVERFLOW)
    {
        SaiAttributeList list(object_type, values, true);

        // no need for id fix since this is overflow
        transfer_attributes(object_type, attr_count, list.get_attr_list(), attr_list, true);
    }

    REDIS_LOG_EXIT();

    return status;
}

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
sai_status_t internal_redis_generic_get(
        _In_ sai_object_type_t object_type,
        _In_ const std::string &serialized_object_id,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    REDIS_LOG_ENTER();

    std::vector<swss::FieldValueTuple> entry = SaiAttributeList::serialize_attr_list(
            object_type, 
            attr_count,
            attr_list,
            false);

    std::string str_object_type;

    sai_serialize_primitive(object_type, str_object_type);

    std::string key = str_object_type + ":" + serialized_object_id;

    g_redisGetProducer->set(key, entry, "get");
    g_redisGetProducer->del(key, "delget");

    // wait for response
    
    swss::Select s;

    s.addSelectable(g_redisGetConsumer);

    while (true)
    {
        swss::Selectable *sel;

        int fd;

        int result = s.select(&sel, &fd, 2000);

        if (result == swss::Select::OBJECT)
        {
            swss::KeyOpFieldsValuesTuple kco;

            g_redisGetConsumer->pop(kco);

            const std::string &op = kfvOp(kco); 
            const std::string &key = kfvKey(kco);

            REDIS_LOG_DBG("op = %s, key = %s", op.c_str(), key.c_str());

            if (op != "getresponse") // ignore non response messages
                continue;

            sai_status_t sai_status = internal_redis_get_process(
                    object_type, 
                    attr_count, 
                    attr_list, 
                    kco);

            REDIS_LOG_EXIT();

            return sai_status;
        }

        REDIS_LOG_ERR("failed to get response for get status: %d", result);
        break;
    }

    REDIS_LOG_EXIT();

    return SAI_STATUS_FAILURE;
}

/**
 * Routine Description:
 *   @brief Generic get attribute
 *
 * Arguments:
 *    @param[in] object_type - the object type
 *    @param[in] object_id - the object id
 *    @param[in] attr_count - number of attributes
 *    @param[out] attr_list - array of attributes
 *
 * Return Values:
 *    @return  SAI_STATUS_SUCCESS on success
 *             Failure status code on error
 */
sai_status_t redis_generic_get(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    REDIS_LOG_ENTER();

    std::string str_object_id;
    sai_serialize_primitive(object_id, str_object_id);

    sai_status_t status = internal_redis_generic_get(
            object_type,
            str_object_id,
            attr_count,
            attr_list);

    REDIS_LOG_EXIT();

    return status;
}

sai_status_t redis_generic_get(
        _In_ sai_object_type_t object_type,
        _In_ const sai_fdb_entry_t *fdb_entry,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    REDIS_LOG_ENTER();

    std::string str_fdb_entry;
    sai_serialize_primitive(*fdb_entry, str_fdb_entry);

    sai_status_t status = internal_redis_generic_get(
            object_type,
            str_fdb_entry,
            attr_count,
            attr_list);

    REDIS_LOG_EXIT();

    return status;
}

sai_status_t redis_generic_get(
        _In_ sai_object_type_t object_type,
        _In_ const sai_neighbor_entry_t* neighbor_entry,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    REDIS_LOG_ENTER();

    std::string str_neighbor_entry;
    sai_serialize_primitive(*neighbor_entry, str_neighbor_entry);

    sai_status_t status = internal_redis_generic_get(
            object_type,
            str_neighbor_entry,
            attr_count,
            attr_list);

    REDIS_LOG_EXIT();

    return status;
}

sai_status_t redis_generic_get(
        _In_ sai_object_type_t object_type,
        _In_ const sai_unicast_route_entry_t* unicast_route_entry,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    REDIS_LOG_ENTER();

    std::string str_route_entry;
    sai_serialize_primitive(*unicast_route_entry, str_route_entry);

    sai_status_t status = internal_redis_generic_get(
            object_type,
            str_route_entry,
            attr_count,
            attr_list);

    REDIS_LOG_EXIT();

    return status;
}

sai_status_t redis_generic_get_vlan(
        _In_ sai_object_type_t object_type,
        _In_ sai_vlan_id_t vlan_id,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    REDIS_LOG_ENTER();

    std::string str_vlan_id;
    sai_serialize_primitive(vlan_id, str_vlan_id);

    sai_status_t status = internal_redis_generic_get(
            object_type,
            str_vlan_id,
            attr_count,
            attr_list);

    REDIS_LOG_EXIT();

    return status;
}
