#include "sai_redis.h"
#include "meta/saiserialize.h"
#include "meta/saiattributelist.h"

// if we don't receive response from syncd in 60 seconds
// there is something wrong and we should fail
#define GET_RESPONSE_TIMEOUT (60*1000)

sai_status_t internal_redis_get_process(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list,
        _In_ swss::KeyOpFieldsValuesTuple &kco)
{
    SWSS_LOG_ENTER();

    // key is: object_type:object_id:sai_status

    const std::string &key = kfvKey(kco);
    const std::vector<swss::FieldValueTuple> &values = kfvFieldsValues(kco);

    std::string str_sai_status = key;

    sai_status_t status;

    sai_deserialize_status(str_sai_status, status);

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

    return status;
}

sai_status_t internal_redis_generic_get(
        _In_ sai_object_type_t object_type,
        _In_ const std::string &serialized_object_id,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_mutex);

    SWSS_LOG_ENTER();

    std::vector<swss::FieldValueTuple> entry = SaiAttributeList::serialize_attr_list(
            object_type,
            attr_count,
            attr_list,
            false);

    std::string str_object_type = sai_serialize_object_type(object_type);

    std::string key = str_object_type + ":" + serialized_object_id;

    SWSS_LOG_DEBUG("generic get key: %s, fields: %lu", key.c_str(), entry.size());

    if (g_record)
    {
        recordLine("g," + key + "," + joinFieldValues(entry));
    }

    // get is special, it will not put data
    // into asic view, only to message queue
    g_asicState->set(key, entry, "get");

    // wait for response

    swss::Select s;

    s.addSelectable(g_redisGetConsumer);

    while (true)
    {
        SWSS_LOG_DEBUG("wait for response");

        swss::Selectable *sel;

        int fd;

        int result = s.select(&sel, &fd, GET_RESPONSE_TIMEOUT);

        if (result == swss::Select::OBJECT)
        {
            swss::KeyOpFieldsValuesTuple kco;

            g_redisGetConsumer->pop(kco);

            const std::string &op = kfvOp(kco);
            const std::string &opkey = kfvKey(kco);

            SWSS_LOG_DEBUG("response: op = %s, key = %s", opkey.c_str(), op.c_str());

            if (op != "getresponse") // ignore non response messages
                continue;

            sai_status_t status = internal_redis_get_process(
                    object_type,
                    attr_count,
                    attr_list,
                    kco);

            if (g_record)
            {
                const std::string &str_status = kfvKey(kco);
                const std::vector<swss::FieldValueTuple> &values = kfvFieldsValues(kco);

                // first serialized is status
                recordLine("G," + str_status + "," + joinFieldValues(values));
            }

            SWSS_LOG_DEBUG("generic get status: %d", status);

            return status;
        }

        SWSS_LOG_ERROR("generic get failed to get response result: %d", result);
        break;
    }

    if (g_record)
    {
        recordLine("G,FAILURE");
    }

    SWSS_LOG_ERROR("generic get failed to get response");

    return SAI_STATUS_FAILURE;
}

sai_status_t redis_generic_get(
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

    std::string str_object_id = sai_serialize_object_id(object_id);

    return internal_redis_generic_get(
            object_type,
            str_object_id,
            attr_count,
            attr_list);
}

sai_status_t redis_generic_get_fdb_entry(
        _In_ const sai_fdb_entry_t *fdb_entry,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    std::string str_fdb_entry = sai_serialize_fdb_entry(*fdb_entry);

    return internal_redis_generic_get(
            SAI_OBJECT_TYPE_FDB,
            str_fdb_entry,
            attr_count,
            attr_list);
}

sai_status_t redis_generic_get_neighbor_entry(
        _In_ const sai_neighbor_entry_t* neighbor_entry,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    std::string str_neighbor_entry = sai_serialize_neighbor_entry(*neighbor_entry);

    return internal_redis_generic_get(
            SAI_OBJECT_TYPE_NEIGHBOR,
            str_neighbor_entry,
            attr_count,
            attr_list);
}

sai_status_t redis_generic_get_route_entry(
        _In_ const sai_unicast_route_entry_t* unicast_route_entry,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    std::string str_route_entry = sai_serialize_route_entry(*unicast_route_entry);

    return internal_redis_generic_get(
            SAI_OBJECT_TYPE_ROUTE,
            str_route_entry,
            attr_count,
            attr_list);
}

sai_status_t redis_generic_get_vlan(
        _In_ sai_vlan_id_t vlan_id,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    std::string str_vlan_id = sai_serialize_vlan_id(vlan_id);

    return internal_redis_generic_get(
            SAI_OBJECT_TYPE_VLAN,
            str_vlan_id,
            attr_count,
            attr_list);
}

sai_status_t redis_generic_get_trap(
        _In_ sai_hostif_trap_id_t hostif_trap_id,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    std::string str_hostif_trap_id = sai_serialize_hostif_trap_id(hostif_trap_id);

    return internal_redis_generic_get(
            SAI_OBJECT_TYPE_TRAP,
            str_hostif_trap_id,
            attr_count,
            attr_list);
}

sai_status_t redis_generic_get_switch(
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_object_id_t object_id = 0;

    std::string str_object_id = sai_serialize_object_id(object_id);

    return internal_redis_generic_get(
            SAI_OBJECT_TYPE_SWITCH,
            str_object_id,
            attr_count,
            attr_list);
}
