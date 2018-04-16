#include "sai_redis.h"
#include "meta/sai_serialize.h"
#include "meta/saiattributelist.h"

sai_status_t internal_redis_get_process(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list,
        _In_ swss::KeyOpFieldsValuesTuple &kco)
{
    SWSS_LOG_ENTER();

    // key:         sai_status
    // field:       attr_id
    // value:       attr_value

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

std::string getSelectResultAsString(int result)
{
    SWSS_LOG_ENTER();

    std::string res;

    switch (result)
    {
        case swss::Select::ERROR:
            res = "ERROR";
            break;

        case swss::Select::TIMEOUT:
            res = "TIMEOUT";
            break;

        default:
            SWSS_LOG_WARN("non recognized select result: %d", result);
            res = std::to_string(result);
            break;
    }

    return res;
}

void clear_oid_values(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    for (uint32_t i = 0; i < attr_count; i++)
    {
        sai_attribute_t &attr = attr_list[i];

        auto meta = sai_metadata_get_attr_metadata(object_type, attr.id);

        if (meta == NULL)
        {
            SWSS_LOG_THROW("unable to get metadata for object type %x, attribute %d", object_type, attr.id);
        }

        sai_object_list_t list = { .count = 0, .list = NULL };

        switch (meta->attrvaluetype)
        {
            case SAI_ATTR_VALUE_TYPE_OBJECT_ID:
                attr.value.oid = SAI_NULL_OBJECT_ID;
                break;

            case SAI_ATTR_VALUE_TYPE_OBJECT_LIST:
                list = attr.value.objlist;
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_ID:
                attr.value.aclfield.data.oid = SAI_NULL_OBJECT_ID;
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_FIELD_DATA_OBJECT_LIST:
                if (attr.value.aclfield.enable)
                    list = attr.value.aclfield.data.objlist;
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_ID:
                    attr.value.aclaction.parameter.oid = SAI_NULL_OBJECT_ID;
                break;

            case SAI_ATTR_VALUE_TYPE_ACL_ACTION_DATA_OBJECT_LIST:
                if (attr.value.aclaction.enable)
                    list = attr.value.aclaction.parameter.objlist;
                break;

            default:

                /*
                 * If in futre new attribute with object id will be added this
                 * will make sure that we will need to add handler here.
                 */

                /*
                 * TODO: Add metadata field for this.
                 */

                if (meta->allowedobjecttypeslength > 0)
                {
                    SWSS_LOG_THROW("attribute %s is object id, but not processed, FIXME", meta->attridname);
                }

                break;
        }

        for (uint32_t idx = 0; idx < list.count; ++idx)
        {
            /*
             * We can do that since list is pointer so actual data will be
             * changed in user buffer.
             */

            list.list[idx] = SAI_NULL_OBJECT_ID;
        }
    }
}

sai_status_t internal_redis_generic_get(
        _In_ sai_object_type_t object_type,
        _In_ const std::string &serialized_object_id,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    /*
     * Since user may reuse buffers, then oid list buffers maybe not cleared
     * and contain som garbage, let's clean them so we send all oids as null to
     * syncd.
     */

    clear_oid_values(object_type, attr_count, attr_list);

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
        recordLine("g|" + key + "|" + joinFieldValues(entry));
    }

    // get is special, it will not put data
    // into asic view, only to message queue
    g_asicState->set(key, entry, "get");

    // wait for response

    swss::Select s;

    s.addSelectable(g_redisGetConsumer.get());

    while (true)
    {
        SWSS_LOG_DEBUG("wait for response");

        swss::Selectable *sel;

        int result = s.select(&sel, GET_RESPONSE_TIMEOUT);

        if (result == swss::Select::OBJECT)
        {
            swss::KeyOpFieldsValuesTuple kco;

            g_redisGetConsumer->pop(kco);

            const std::string &op = kfvOp(kco);
            const std::string &opkey = kfvKey(kco);

            SWSS_LOG_DEBUG("response: op = %s, key = %s", opkey.c_str(), op.c_str());

            if (op != "getresponse") // ignore non response messages
            {
                continue;
            }

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
                recordLine("G|" + str_status + "|" + joinFieldValues(values));
            }

            SWSS_LOG_DEBUG("generic get status: %d", status);

            return status;
        }

        SWSS_LOG_ERROR("generic get failed due to SELECT operation result: %s", getSelectResultAsString(result).c_str());
        break;
    }

    if (g_record)
    {
        recordLine("G|SAI_STATUS_FAILURE");
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
            SAI_OBJECT_TYPE_FDB_ENTRY,
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
            SAI_OBJECT_TYPE_NEIGHBOR_ENTRY,
            str_neighbor_entry,
            attr_count,
            attr_list);
}

sai_status_t redis_generic_get_route_entry(
        _In_ const sai_route_entry_t* route_entry,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    std::string str_route_entry = sai_serialize_route_entry(*route_entry);

    return internal_redis_generic_get(
            SAI_OBJECT_TYPE_ROUTE_ENTRY,
            str_route_entry,
            attr_count,
            attr_list);
}
