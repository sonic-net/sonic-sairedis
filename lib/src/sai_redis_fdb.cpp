#include "sai_redis.h"
#include "meta/sai_serialize.h"
#include "meta/saiattributelist.h"

sai_status_t internal_redis_flush_fdb_entries(
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    std::vector<swss::FieldValueTuple> entry = SaiAttributeList::serialize_attr_list(
            SAI_OBJECT_TYPE_FDB_FLUSH,
            attr_count,
            attr_list,
            false);

    std::string str_object_type = sai_serialize_object_type(SAI_OBJECT_TYPE_FDB_FLUSH);

    std::string key = str_object_type + ":" + sai_serialize_object_id(switch_id);

    SWSS_LOG_NOTICE("flush key: %s, fields: %lu", key.c_str(), entry.size());

    if (g_record)
    {
        recordLine("f|" + key + "|" + joinFieldValues(entry));
    }

    // flush is special, it will not put data
    // into asic view, only to message queue
    g_asicState->set(key, entry, "flush");

    // wait for response

    swss::Select s;

    // get consumer will be reused for flush

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

            if (op != "flushresponse") // ignore non response messages
            {
                continue;
            }

            std::string str_sai_status = opkey;

            sai_status_t status;

            sai_deserialize_status(str_sai_status, status);

            if (g_record)
            {
                const std::string &str_status = kfvKey(kco);

                // first serialized is status
                recordLine("F|" + str_status);
            }

            SWSS_LOG_NOTICE("flush status: %d", status);

            return status;
        }

        SWSS_LOG_ERROR("flush failed due to SELECT operation result: %s", getSelectResultAsString(result).c_str());
        break;
    }

    if (g_record)
    {
        recordLine("F|SAI_STATUS_FAILURE");
    }

    SWSS_LOG_ERROR("flush failed to get response");

    return SAI_STATUS_FAILURE;
}

sai_status_t redis_flush_fdb_entries(
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return meta_sai_flush_fdb_entries(
            switch_id,
            attr_count,
            attr_list,
            internal_redis_flush_fdb_entries);
}

REDIS_GENERIC_QUAD_ENTRY(FDB_ENTRY,fdb_entry);

const sai_fdb_api_t redis_fdb_api = {

    REDIS_GENERIC_QUAD_API(fdb_entry)

    redis_flush_fdb_entries,
};

sai_status_t redis_dummy_create_fdb_entry(
        _In_ const sai_fdb_entry_t *fdb_entry,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    /*
     * Since we are using validation for each fdb in bulk operations, we
     * can't execute actual CREATE, we need to do dummy create and then introduce
     * internal bulk_create operation that will only touch redis db only once.
     * So we are returning success here.
     */

    return SAI_STATUS_SUCCESS;
}

sai_status_t redis_bulk_create_fdb_entry(
        _In_ uint32_t object_count,
        _In_ const sai_fdb_entry_t *fdb_entry,
        _In_ const uint32_t *attr_count,
        _In_ const sai_attribute_t *const *attr_list,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (object_count < 1)
    {
        SWSS_LOG_ERROR("expected at least 1 object to create");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (fdb_entry == NULL)
    {
        SWSS_LOG_ERROR("fdb_entry is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_count == NULL)
    {
        SWSS_LOG_ERROR("attr_count is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_list == NULL)
    {
        SWSS_LOG_ERROR("attr_list is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    switch (mode)
    {
        case SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR:
        case SAI_BULK_OP_ERROR_MODE_IGNORE_ERROR:
             // ok
             break;

        default:

             SWSS_LOG_ERROR("invalid bulk operation mode %d", mode);

             return SAI_STATUS_INVALID_PARAMETER;
    }

    if (object_statuses == NULL)
    {
        SWSS_LOG_ERROR("object_statuses is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    std::vector<std::string> serialized_object_ids;

    for (uint32_t idx = 0; idx < object_count; ++idx)
    {
        /*
         * At the beginning set all statuses to not executed.
         */

        object_statuses[idx] = SAI_STATUS_NOT_EXECUTED;

        serialized_object_ids.push_back(
                sai_serialize_fdb_entry(fdb_entry[idx]));
    }

    for (uint32_t idx = 0; idx < object_count; ++idx)
    {
        sai_status_t status =
            meta_sai_create_fdb_entry(
                    &fdb_entry[idx],
                    attr_count[idx],
                    attr_list[idx],
                    &redis_dummy_create_fdb_entry);

        object_statuses[idx] = status;

        if (status != SAI_STATUS_SUCCESS)
        {
            // TODO add attr id and value

            SWSS_LOG_ERROR("failed on index %u: %s",
                    idx,
                    serialized_object_ids[idx].c_str());

            if (mode == SAI_BULK_OP_ERROR_MODE_STOP_ON_ERROR)
            {
                SWSS_LOG_NOTICE("stop on error since previous operation failed");
                break;
            }
        }
    }

    /*
     * TODO: we need to record operation type
     */

    return internal_redis_bulk_generic_create(
            SAI_OBJECT_TYPE_ROUTE_ENTRY,
            serialized_object_ids,
            attr_count,
            attr_list,
            object_statuses);
}

sai_status_t redis_bulk_remove_fdb_entry(
        _In_ uint32_t object_count,
        _In_ const sai_fdb_entry_t *fdb_entry,
        _In_ sai_bulk_op_error_mode_t mode,
        _Out_ sai_status_t *object_statuses)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    std::vector<std::string> serialized_object_ids;

    for (uint32_t idx = 0; idx < object_count; ++idx)
    {
        /*
         * At the beginning set all statuses to not executed.
         */

        object_statuses[idx] = SAI_STATUS_NOT_EXECUTED;

        serialized_object_ids.push_back(
                sai_serialize_fdb_entry(fdb_entry[idx]));
    }

    return internal_redis_bulk_generic_remove(SAI_OBJECT_TYPE_ROUTE_ENTRY, serialized_object_ids, object_statuses);
}
