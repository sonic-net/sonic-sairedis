#include "sai_redis.h"
#include "meta/sai_serialize.h"

volatile bool g_recordStats = true;

/*
 * Max number of counters used in 1 api call
 */
#define REDIS_MAX_COUNTERS 128

#define REDIS_COUNTERS_COUNT_MSB (0x80000000)

sai_status_t internal_redis_generic_stats_function(
        _In_ sai_object_type_t obejct_type,
        _In_ sai_object_id_t object_id,
        _In_ sai_object_id_t switch_id,
        _In_ const sai_enum_metadata_t *enum_metadata,
        _In_ uint32_t number_of_counters,
        _In_ const int32_t *counter_ids,
        _In_ sai_stats_mode_t mode,
        _Out_ uint64_t *counters)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t redis_generic_stats_function(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ const sai_enum_metadata_t *enum_metadata,
        _In_ uint32_t number_of_counters,
        _In_ const int32_t *counter_ids,
        _In_ sai_stats_mode_t mode,
        _Out_ uint64_t *counters)
{
    SWSS_LOG_ENTER();

    /*
     * Do all parameter validation.
     */

    if (object_id == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_ERROR("object id is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_object_type_t ot = g_virtualObjectIdManager->saiObjectTypeQuery(object_id);

    if (ot != object_type)
    {
        SWSS_LOG_ERROR("object %s is %s but expected %s",
                sai_serialize_object_id(object_id).c_str(),
                sai_serialize_object_type(ot).c_str(),
                sai_serialize_object_type(object_type).c_str());

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_object_id_t switch_id = g_virtualObjectIdManager->saiSwitchIdQuery(object_id);

    if (switch_id == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_ERROR("object %s does not correspond to any switch object",
                sai_serialize_object_id(object_id).c_str());

        return SAI_STATUS_INVALID_PARAMETER;
    }

    uint32_t count = number_of_counters & ~REDIS_COUNTERS_COUNT_MSB;

    if (count > REDIS_MAX_COUNTERS)
    {
        SWSS_LOG_ERROR("max supported counters to get/clear is %u, but %u given",
                REDIS_MAX_COUNTERS,
                count);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (counter_ids == NULL)
    {
        SWSS_LOG_ERROR("counter ids pointer is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (counters == NULL)
    {
        SWSS_LOG_ERROR("counters output pointer is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (enum_metadata == NULL)
    {
        SWSS_LOG_ERROR("enum metadata pointer is NULL, bug?");

        return SAI_STATUS_FAILURE;
    }

    for (uint32_t i = 0; i < count; i++)
    {
        if (sai_metadata_get_enum_value_name(enum_metadata, counter_ids[i]) == NULL)
        {
            SWSS_LOG_ERROR("counter id %u is not allowed on %s", counter_ids[i], enum_metadata->name);

            return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    switch (mode)
    {
        case SAI_STATS_MODE_READ:
        case SAI_STATS_MODE_READ_AND_CLEAR:
            break;

        default:

            SWSS_LOG_ERROR("counters mode is invalid %d", mode);

            return SAI_STATUS_INVALID_PARAMETER;
    }

    return internal_redis_generic_stats_function(
            object_type,
            object_id,
            switch_id,
            enum_metadata,
            number_of_counters,
            counter_ids,
            mode,
            counters);
}

sai_status_t internal_redis_get_stats_process(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t count,
        _Out_ uint64_t *counter_list,
        _In_ swss::KeyOpFieldsValuesTuple &kco)
{
    SWSS_LOG_ENTER();

    // key:         sai_status
    // field:       stat_id
    // value:       stat_value

    const auto &key = kfvKey(kco);
    const auto &values = kfvFieldsValues(kco);

    auto str_sai_status = key;

    sai_status_t status;

    sai_deserialize_status(str_sai_status, status);

    if (status == SAI_STATUS_SUCCESS)
    {
        uint32_t i = 0;
        for (const auto &v : values)
        {
            if (i >= count)
            {
                SWSS_LOG_ERROR("Received more values than expected");
                status = SAI_STATUS_FAILURE;
                break;
            }

            uint64_t value = 0;

            value = stoull(fvValue(v));
            counter_list[i] = value;
            i++;
        }
    }

    return status;
}

sai_status_t internal_redis_clear_stats_process(
        _In_ sai_object_type_t object_type,
        _In_ const sai_enum_metadata_t *stats_enum_metadata,
        _In_ uint32_t count,
        _In_ const int32_t *counter_id_list,
        _In_ swss::KeyOpFieldsValuesTuple &kco)
{
    SWSS_LOG_ENTER();

    // key:         sai_status
    // field:       stat_id

    const auto &key = kfvKey(kco);
    sai_status_t status;
    sai_deserialize_status(key, status);
    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    const auto &fvTuples = kfvFieldsValues(kco);
    if (fvTuples.size() != count)
    {
        SWSS_LOG_ERROR("Field count %zu not as expected %u", fvTuples.size(), count);
        return SAI_STATUS_FAILURE;
    }

    int32_t counter_id;
    for (uint32_t i = 0; i < count; i++)
    {
        sai_deserialize_enum(fvField(fvTuples[i]).c_str(), stats_enum_metadata, &counter_id);
        if (counter_id != counter_id_list[i])
        {
            SWSS_LOG_ERROR("Counter id %s not as expected %s",
                fvField(fvTuples[i]).c_str(),
                sai_serialize_enum(counter_id_list[i], stats_enum_metadata).c_str());
            status = SAI_STATUS_FAILURE;
            break;
        }
    }

    return status;
}

std::vector<swss::FieldValueTuple> serialize_counter_id_list(
        _In_ const sai_enum_metadata_t *stats_enum,
        _In_ uint32_t count,
        _In_ const int32_t *counter_id_list)
{
    SWSS_LOG_ENTER();

    std::vector<swss::FieldValueTuple> values;

    for (uint32_t i = 0; i < count; i++)
    {
        const char *name = sai_metadata_get_enum_value_name(stats_enum, counter_id_list[i]);

        if (name == NULL)
        {
            SWSS_LOG_THROW("failed to find enum %d in %s", counter_id_list[i], stats_enum->name);
        }

        values.emplace_back(name, "");
    }

    return values;
}

sai_status_t internal_redis_generic_get_stats(
        _In_ sai_object_type_t object_type,
        _In_ const std::string &serialized_object_id,
        _In_ const sai_enum_metadata_t *stats_enum,
        _In_ uint32_t count,
        _In_ const int32_t *counter_id_list,
        _Out_ uint64_t *counter_list)
{
    SWSS_LOG_ENTER();

    std::vector<swss::FieldValueTuple> entry = serialize_counter_id_list(
            stats_enum,
            count,
            counter_id_list);

    std::string str_object_type = sai_serialize_object_type(object_type);

    std::string key = str_object_type + ":" + serialized_object_id;

    SWSS_LOG_DEBUG("generic get stats key: %s, fields: %lu", key.c_str(), entry.size());

    if (g_record && g_recordStats)
    {
        recordLine("m|" + key + "|" + joinFieldValues(entry));
    }

    // get is special, it will not put data
    // into asic view, only to message queue
    g_asicState->set(key, entry, "get_stats");

    // wait for response

    swss::Select s;

    s.addSelectable(g_redisGetConsumer.get());

    while (true)
    {
        SWSS_LOG_DEBUG("wait for get_stats response");

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

            sai_status_t status = internal_redis_get_stats_process(
                    object_type,
                    count,
                    counter_list,
                    kco);

            if (g_record && g_recordStats)
            {
                const auto &str_status = kfvKey(kco);
                const auto &values = kfvFieldsValues(kco);

                // first serialized is status
                recordLine("M|" + str_status + "|" + joinFieldValues(values));
            }

            SWSS_LOG_DEBUG("generic get status: %d", status);

            return status;
        }

        SWSS_LOG_ERROR("generic get failed due to SELECT operation result");
        break;
    }

    if (g_record && g_recordStats)
    {
        recordLine("M|SAI_STATUS_FAILURE");
    }

    SWSS_LOG_ERROR("generic get stats failed to get response");

    return SAI_STATUS_FAILURE;
}

sai_status_t redis_generic_get_stats(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ const sai_enum_metadata_t *stats_enum,
        _In_ uint32_t number_of_counters,
        _In_ const int32_t *counter_ids,
        _Out_ uint64_t *counters)
{
    SWSS_LOG_ENTER();

    std::string str_object_id = sai_serialize_object_id(object_id);

    return internal_redis_generic_get_stats(
            object_type,
            str_object_id,
            stats_enum,
            number_of_counters,
            counter_ids,
            counters);
}

sai_status_t redis_generic_get_stats_ext(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ const sai_enum_metadata_t *enum_metadata,
        _In_ uint32_t number_of_counters,
        _In_ const int32_t *counter_ids,
        _In_ sai_stats_mode_t mode,
        _Out_ uint64_t *counters)
{
    SWSS_LOG_ENTER();

    return redis_generic_stats_function(
            object_type,
            object_id,
            enum_metadata,
            number_of_counters,
            counter_ids,
            mode,
            counters);
}

sai_status_t internal_redis_generic_clear_stats(
        _In_ sai_object_type_t object_type,
        _In_ const std::string &serialized_object_id,
        _In_ const sai_enum_metadata_t *stats_enum,
        _In_ uint32_t count,
        _In_ const int32_t *counter_id_list)
{
    SWSS_LOG_ENTER();

    std::vector<swss::FieldValueTuple> fvTuples = serialize_counter_id_list(
            stats_enum,
            count,
            counter_id_list);

    std::string str_object_type = sai_serialize_object_type(object_type);
    std::string key = str_object_type + ":" + serialized_object_id;

    SWSS_LOG_DEBUG("generic clear stats key: %s, fields: %lu", key.c_str(), fvTuples.size());

    if (g_record && g_recordStats)
    {
        recordLine("m|" + key + "|" + joinFieldValues(fvTuples));
    }

    // clear is special, it will not put data
    // into asic view, only to message queue
    g_asicState->set(key, fvTuples, "clear_stats");

    // wait for response
    swss::Select s;
    s.addSelectable(g_redisGetConsumer.get());
    while (true)
    {
        SWSS_LOG_DEBUG("wait for clear_stats response");

        swss::Selectable *sel;
        int result = s.select(&sel, GET_RESPONSE_TIMEOUT);
        if (result == swss::Select::OBJECT)
        {
            swss::KeyOpFieldsValuesTuple kco;
            g_redisGetConsumer->pop(kco);
            const std::string &respKey = kfvKey(kco);
            const std::string &respOp = kfvOp(kco);
            SWSS_LOG_DEBUG("response: key = %s, op = %s", respKey.c_str(), respOp.c_str());

            if (respOp != "getresponse") // ignore non response messages
            {
                continue;
            }

            if (g_record && g_recordStats)
            {
                const auto &respFvTuples = kfvFieldsValues(kco);

                // first serialized is status return by sai clear_stats
                recordLine("M|" + respKey + "|" + joinFieldValues(respFvTuples));
            }

            sai_status_t status = internal_redis_clear_stats_process(
                    object_type,
                    stats_enum,
                    count,
                    counter_id_list,
                    kco);
            SWSS_LOG_DEBUG("generic clear stats status: %s", sai_serialize_status(status).c_str());
            return status;
        }

        SWSS_LOG_ERROR("generic clear stats failed due to SELECT operation result");
        break;
    }

    if (g_record && g_recordStats)
    {
        recordLine("M|SAI_STATUS_FAILURE");
    }

    SWSS_LOG_ERROR("generic clear stats failed to get response");
    return SAI_STATUS_FAILURE;
}

sai_status_t redis_generic_clear_stats(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ const sai_enum_metadata_t *enum_metadata_stats,
        _In_ uint32_t number_of_counters,
        _In_ const int32_t *counter_ids)
{
    SWSS_LOG_ENTER();

    std::string str_object_id = sai_serialize_object_id(object_id);

    return internal_redis_generic_clear_stats(
            object_type,
            str_object_id,
            enum_metadata_stats,
            number_of_counters,
            counter_ids);
}
