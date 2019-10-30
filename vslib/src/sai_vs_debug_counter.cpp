#include "sai_vs.h"
#include "sai_vs_internal.h"
#include "sai_vs_state.h"
#include <unordered_set>

const int maxDebugCounters = 32;

static std::unordered_set<uint32_t> indices;

static uint32_t get_index()
{
    SWSS_LOG_ENTER();

    for (uint32_t i = 0; i < maxDebugCounters; i++)
    {
        if (indices.find(i) == indices.end())
        {
            indices.insert(i);
            return i;
        }
    }

    return UINT32_MAX;
}

sai_status_t vs_create_debug_counter(
        _Out_ sai_object_id_t *debug_counter_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    MUTEX();

    SWSS_LOG_ENTER();

    auto index = get_index();

    if (index > maxDebugCounters)
    {
        SWSS_LOG_ERROR("Cannot create any more debug counters");
        return SAI_STATUS_FAILURE;
    }

    CHECK_STATUS(meta_sai_create_oid(
            static_cast<sai_object_type_t>(SAI_OBJECT_TYPE_DEBUG_COUNTER),
            debug_counter_id,
            switch_id,
            attr_count,
            attr_list,
            &vs_generic_create));

    sai_attribute_t attr;
    attr.id = SAI_DEBUG_COUNTER_ATTR_INDEX;
    attr.value.u32 = index;
    CHECK_STATUS(vs_generic_set(SAI_OBJECT_TYPE_DEBUG_COUNTER, *debug_counter_id, &attr));

    return SAI_STATUS_SUCCESS;
}

sai_status_t vs_remove_debug_counter(
        _In_ sai_object_id_t debug_counter_id)
{
    MUTEX();
    SWSS_LOG_ENTER();

    sai_attribute_t attr;
    attr.id = SAI_DEBUG_COUNTER_ATTR_INDEX;
    CHECK_STATUS(vs_generic_get(SAI_OBJECT_TYPE_DEBUG_COUNTER, debug_counter_id, 1, &attr));

    CHECK_STATUS(meta_sai_remove_oid(
            (sai_object_type_t)SAI_OBJECT_TYPE_DEBUG_COUNTER,
            debug_counter_id,
            &vs_generic_remove));

    indices.erase(attr.value.u32);

    return SAI_STATUS_SUCCESS;
}

VS_GET(DEBUG_COUNTER,debug_counter);
VS_SET(DEBUG_COUNTER,debug_counter);

const sai_debug_counter_api_t vs_debug_counter_api = {
    VS_GENERIC_QUAD_API(debug_counter)
};
