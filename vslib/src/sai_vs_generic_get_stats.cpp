#include "sai_vs.h"
#include "sai_vs_state.h"
#include "meta/sai_serialize.h"

template <typename T>
sai_status_t internal_vs_generic_get_stats(
        _In_ sai_object_type_t object_type,
        _In_ const std::string &serialized_object_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t count,
        _In_ const T *counter_id_list,
        _Out_ uint64_t *counter_list)
{
    SWSS_LOG_ENTER();

    bool perform_set = false;

    if (meta_unittests_enabled() && (count & 0x80000000))
    {
        count = count & ~0x80000000;

        SWSS_LOG_NOTICE("unittests are enabled and counters count MSB is set to 1, performing SET on counters");

        perform_set = true;
    }

    auto &countersMap = g_switch_state_map.at(switch_id)->countersMap;

    auto it = countersMap.find(serialized_object_id);

    if (it == countersMap.end())
        countersMap[serialized_object_id] = std::map<int,uint64_t>();

    std::map<int,uint64_t>& counters = countersMap[serialized_object_id];

    for (uint32_t i = 0; i < count; ++i)
    {
        if (perform_set)
        {
            counters[ counter_id_list[i] ] = counter_list[i]; 
        }
        else
        {
            auto itt = counters.find(counter_id_list[i]);

            if (itt == counters.end()) 
                counter_list[i] = 0;
            else
                counter_list[i] = itt->second;
        }
    }

    return SAI_STATUS_SUCCESS;
}

template <typename T>
sai_status_t vs_generic_get_stats(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t count,
        _In_ const T* counter_id_list,
        _Out_ uint64_t *counter_list)
{
    SWSS_LOG_ENTER();

    std::string str_object_id = sai_serialize_object_id(object_id);

    sai_object_id_t switch_id = sai_switch_id_query(object_id);

    return internal_vs_generic_get_stats(
            object_type,
            str_object_id,
            switch_id,
            count,
            counter_id_list,
            counter_list);
}

#define DECLARE_VS_GENERIC_GET_STATS(type)                                \
    template                                                              \
    sai_status_t vs_generic_get_stats<sai_ ## type ## _stat_t>(           \
        _In_ sai_object_type_t object_type,                               \
        _In_ sai_object_id_t object_id,                                   \
        _In_ uint32_t count,                                              \
        _In_ const sai_ ## type ## _stat_t *counter_id_list,              \
        _Out_ uint64_t *counter_list);                                    \

DECLARE_VS_GENERIC_GET_STATS(port);
DECLARE_VS_GENERIC_GET_STATS(port_pool);
DECLARE_VS_GENERIC_GET_STATS(queue);
DECLARE_VS_GENERIC_GET_STATS(ingress_priority_group);
