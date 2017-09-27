#include "sai_vs.h"
#include "sai_vs_internal.h"

sai_status_t vs_get_policer_stats(
        _In_ sai_object_id_t policer_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_policer_stat_t *counter_ids,
        _Out_ uint64_t *counters)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t vs_clear_policer_stats(
        _In_ sai_object_id_t policer_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_policer_stat_t *counter_ids)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

VS_GENERIC_QUAD(POLICER,policer);

const sai_policer_api_t vs_policer_api = {

    VS_GENERIC_QUAD_API(policer)

    vs_get_policer_stats,
    vs_clear_policer_stats,
};
