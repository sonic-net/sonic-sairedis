#include "sai_redis.h"
#include "meta/saiserialize.h"

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

    const std::string &key = kfvKey(kco);
    const std::vector<swss::FieldValueTuple> &values = kfvFieldsValues(kco);

    std::string str_sai_status = key;

    sai_status_t status;

    sai_deserialize_status(str_sai_status, status);

    if (status == SAI_STATUS_SUCCESS)
    {
        uint32_t i = 0;
        for (const auto &v : values)
        {
            uint64_t value = 0;

            value = stoull(fvValue(v));
            counter_list[i] = value;
            i++;
        }
    }

    return status;
}

// Another way to implement this would be to pass sai_serialize_ ## type ## stat as a parameter,
// but the decision point of what function to pass is up in sai_redis_internal.h, which would make us
// pass a callback through about five function calls, so for the sake of simplicity here's a macro that
// generates overloaded functions
#define DECLARE_SERIALIZE_COUNTER_ID_LIST(type)                                                               \
    std::vector<swss::FieldValueTuple> serialize_counter_id_list(                                             \
            _In_ uint32_t count,                                                                              \
            _In_ const sai_ ## type ## _stat_t *counter_id_list)                                              \
    {                                                                                                         \
        SWSS_LOG_ENTER();                                                                                     \
                                                                                                              \
        std::vector<swss::FieldValueTuple> values;                                                            \
                                                                                                              \
        for (uint32_t i = 0; i < count; i++)                                                                  \
        {                                                                                                     \
            std::string field = sai_serialize_ ## type ## _stat(counter_id_list[i]);                          \
            values.emplace_back(field, "");                                                                   \
        }                                                                                                     \
                                                                                                              \
        return std::move(values);                                                                             \
    }

DECLARE_SERIALIZE_COUNTER_ID_LIST(port);
DECLARE_SERIALIZE_COUNTER_ID_LIST(queue);
DECLARE_SERIALIZE_COUNTER_ID_LIST(ingress_priority_group);

template <typename T>
sai_status_t internal_redis_generic_get_stats(
        _In_ sai_object_type_t object_type,
        _In_ const std::string &serialized_object_id,
        _In_ uint32_t count,
        _In_ const T *counter_id_list,
        _Out_ uint64_t *counter_list)
{
    SWSS_LOG_ENTER();

    std::vector<swss::FieldValueTuple> entry = serialize_counter_id_list(
            count,
            counter_id_list);

    std::string str_object_type = sai_serialize_object_type(object_type);

    std::string key = str_object_type + ":" + serialized_object_id;

    SWSS_LOG_DEBUG("generic get stats key: %s, fields: %lu", key.c_str(), entry.size());

    if (g_record)
    {
        // XXX don't know which character to use for get stats. For now its 'm'
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
            {
                continue;
            }

            sai_status_t status = internal_redis_get_stats_process(
                    object_type,
                    count,
                    counter_list,
                    kco);

            if (g_record)
            {
                const std::string &str_status = kfvKey(kco);
                const std::vector<swss::FieldValueTuple> &values = kfvFieldsValues(kco);

                // first serialized is status
                recordLine("M|" + str_status + "|" + joinFieldValues(values));
            }

            SWSS_LOG_DEBUG("generic get status: %d", status);

            return status;
        }

        SWSS_LOG_ERROR("generic get failed due to SELECT operation result");
        break;
    }

    if (g_record)
    {
        recordLine("M|SAI_STATUS_FAILURE");
    }

    SWSS_LOG_ERROR("generic get stats failed to get response");

    return SAI_STATUS_FAILURE;
}

template <typename T>
sai_status_t redis_generic_get_stats(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t count,
        _In_ const T* counter_id_list,
        _Out_ uint64_t *counter_list)
{
    SWSS_LOG_ENTER();

    std::string str_object_id = sai_serialize_object_id(object_id);

    return internal_redis_generic_get_stats(
            object_type,
            str_object_id,
            count,
            counter_id_list,
            counter_list);
}

#define DECLARE_REDIS_GENERIC_GET_STATS(type)                             \
    template                                                              \
    sai_status_t redis_generic_get_stats<sai_ ## type ## _stat_t>(        \
        _In_ sai_object_type_t object_type,                               \
        _In_ sai_object_id_t object_id,                                   \
        _In_ uint32_t count,                                              \
        _In_ const sai_ ## type ## _stat_t *counter_id_list,              \
        _Out_ uint64_t *counter_list);                                    \

DECLARE_REDIS_GENERIC_GET_STATS(port);
DECLARE_REDIS_GENERIC_GET_STATS(queue);
DECLARE_REDIS_GENERIC_GET_STATS(ingress_priority_group);
