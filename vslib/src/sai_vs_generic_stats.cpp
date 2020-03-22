#include "sai_vs.h"
#include "sai_vs_state.h"
#include "meta/sai_serialize.h"

/*
 * Max number of counters used in 1 api call
 */
#define VS_MAX_COUNTERS 128

#define VS_COUNTERS_COUNT_MSB (0x80000000)

sai_status_t internal_vs_generic_stats_function(
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

    bool perform_set = false;

    if (meta_unittests_enabled() && (number_of_counters & VS_COUNTERS_COUNT_MSB ))
    {
        number_of_counters &= ~VS_COUNTERS_COUNT_MSB;

        SWSS_LOG_NOTICE("unittests are enabled and counters count MSB is set to 1, performing SET on %s counters (%s)",
                sai_serialize_object_id(object_id).c_str(),
                enum_metadata->name);

        perform_set = true;
    }

    auto &countersMap = g_switch_state_map.at(switch_id)->countersMap;

    auto str_object_id = sai_serialize_object_id(object_id);

    auto mapit = countersMap.find(str_object_id);

    if (mapit == countersMap.end())
        countersMap[str_object_id] = std::map<int,uint64_t>();

    std::map<int,uint64_t>& localcounters = countersMap[str_object_id];

    for (uint32_t i = 0; i < number_of_counters; ++i)
    {
        int32_t id = counter_ids[i];

        if (perform_set)
        {
            localcounters[ id ] = counters[i];
        }
        else
        {
            auto it = localcounters.find(id);

            if (it == localcounters.end())
            {
                // if counter is not found on list, just return 0
                counters[i] = 0;
            }
            else
                counters[i] = it->second;

            if (mode == SAI_STATS_MODE_READ_AND_CLEAR)
            {
                localcounters[ id ] = 0;
            }
        }
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t vs_generic_stats_function(
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

    sai_object_type_t ot = sai_object_type_query(object_id);

    if (ot != object_type)
    {
        SWSS_LOG_ERROR("object %s is %s but expected %s",
                sai_serialize_object_id(object_id).c_str(),
                sai_serialize_object_type(ot).c_str(),
                sai_serialize_object_type(object_type).c_str());

        return SAI_STATUS_INVALID_PARAMETER;
    }

    sai_object_id_t switch_id = sai_switch_id_query(object_id);

    if (switch_id == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_ERROR("object %s does not correspond to any switch object",
                sai_serialize_object_id(object_id).c_str());

        return SAI_STATUS_INVALID_PARAMETER;
    }

    uint32_t count = number_of_counters & ~VS_COUNTERS_COUNT_MSB;

    if (count > VS_MAX_COUNTERS)
    {
        SWSS_LOG_ERROR("max supported counters to get/clear is %u, but %u given",
                VS_MAX_COUNTERS,
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

    return internal_vs_generic_stats_function(
            object_type,
            object_id,
            switch_id,
            enum_metadata,
            number_of_counters,
            counter_ids,
            mode,
            counters);
}

sai_status_t vs_generic_get_stats(
    _In_ sai_object_type_t object_type,
    _In_ sai_object_id_t object_id,
    _In_ const sai_enum_metadata_t *enum_metadata,
    _In_ uint32_t number_of_counters,
    _In_ const int32_t *counter_ids,
    _Out_ uint64_t *counters)
{
    SWSS_LOG_ENTER();

    /*
     * Get stats is the same as get stats ext with mode == SAI_STATS_MODE_READ.
     */

    return vs_generic_stats_function(
            object_type,
            object_id,
            enum_metadata,
            number_of_counters,
            counter_ids,
            SAI_STATS_MODE_READ,
            counters);
}

sai_status_t vs_generic_get_stats_ext(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ const sai_enum_metadata_t *enum_metadata,
        _In_ uint32_t number_of_counters,
        _In_ const int32_t *counter_ids,
        _In_ sai_stats_mode_t mode,
        _Out_ uint64_t *counters)
{
    SWSS_LOG_ENTER();

    return vs_generic_stats_function(
            object_type,
            object_id,
            enum_metadata,
            number_of_counters,
            counter_ids,
            mode,
            counters);
}

sai_status_t vs_generic_clear_stats(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ const sai_enum_metadata_t *enum_metadata,
        _In_ uint32_t number_of_counters,
        _In_ const int32_t *counter_ids)
{
    SWSS_LOG_ENTER();

    /*
     * Clear stats is the same as get stats ext with mode ==
     * SAI_STATS_MODE_READ_AND_CLEAR and we just read counters locally and
     * discard them, in that way.
     */

    uint64_t counters[VS_MAX_COUNTERS];

    return vs_generic_stats_function(
            object_type,
            object_id,
            enum_metadata,
            number_of_counters,
            counter_ids,
            SAI_STATS_MODE_READ_AND_CLEAR,
            counters);
}
