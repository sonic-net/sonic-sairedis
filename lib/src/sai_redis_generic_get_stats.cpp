#include "sai_redis.h"
#include "meta/sai_serialize.h"

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

template <class T> struct stat_traits {};

template <>
struct stat_traits<sai_port_stat_t>
{
    typedef std::string (*serialize_stat)(sai_port_stat_t);
    static constexpr serialize_stat serialize_stat_fn = sai_serialize_port_stat;
};

template <>
struct stat_traits<sai_port_pool_stat_t>
{
    typedef std::string (*serialize_stat)(sai_port_pool_stat_t);
    static constexpr serialize_stat serialize_stat_fn = sai_serialize_port_pool_stat;
};

template <>
struct stat_traits<sai_queue_stat_t>
{
    typedef std::string (*serialize_stat)(sai_queue_stat_t);
    static constexpr serialize_stat serialize_stat_fn = sai_serialize_queue_stat;
};

template <>
struct stat_traits<sai_ingress_priority_group_stat_t>
{
    typedef std::string (*serialize_stat)(sai_ingress_priority_group_stat_t);
    static constexpr serialize_stat serialize_stat_fn = sai_serialize_ingress_priority_group_stat;
};

template <>
struct stat_traits<sai_tunnel_stat_t>
{
    typedef std::string (*serialize_stat)(sai_tunnel_stat_t);
    static constexpr serialize_stat serialize_stat_fn = sai_serialize_tunnel_stat;
};


template <class T>
std::vector<swss::FieldValueTuple> serialize_counter_id_list(
        _In_ uint32_t count,
        _In_ const T *counter_id_list)
{
    SWSS_LOG_ENTER();

    std::vector<swss::FieldValueTuple> values;

    for (uint32_t i = 0; i < count; i++)
    {
        std::string field = stat_traits<T>::serialize_stat_fn(counter_id_list[i]);
        values.emplace_back(field, "");
    }

    return std::move(values);
}

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

            if (g_record)
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
DECLARE_REDIS_GENERIC_GET_STATS(port_pool);
DECLARE_REDIS_GENERIC_GET_STATS(queue);
DECLARE_REDIS_GENERIC_GET_STATS(ingress_priority_group);
DECLARE_REDIS_GENERIC_GET_STATS(tunnel);
