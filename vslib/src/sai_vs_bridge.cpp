#include "sai_vs.h"
#include "sai_vs_internal.h"

sai_status_t vs_get_bridge_stats(
        _In_ sai_object_id_t bridge_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_bridge_stat_t *counter_ids,
        _Out_ uint64_t *counters)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t vs_clear_bridge_stats(
        _In_ sai_object_id_t bridge_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_bridge_stat_t *counter_ids)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t vs_get_bridge_port_stats(
        _In_ sai_object_id_t bridge_port_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_bridge_port_stat_t *counter_ids,
        _Out_ uint64_t *counters)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t vs_clear_bridge_port_stats(
        _In_ sai_object_id_t bridge_port_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_bridge_port_stat_t *counter_ids)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

VS_GENERIC_QUAD(BRIDGE,bridge);
VS_GENERIC_QUAD(BRIDGE_PORT,bridge_port);

const sai_bridge_api_t vs_bridge_api =
{
    VS_GENERIC_QUAD_API(bridge)

    vs_get_bridge_stats,
    vs_clear_bridge_stats,

    VS_GENERIC_QUAD_API(bridge_port)

    vs_get_bridge_port_stats,
    vs_clear_bridge_port_stats,
};
