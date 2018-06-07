#include "sai_vs.h"
#include "sai_vs_internal.h"

sai_status_t vs_get_tunnel_stats(
        _In_ sai_object_id_t tunnel_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_tunnel_stat_t *counter_ids,
        _Out_ uint64_t *counters)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t vs_get_tunnel_stats_ext(
        _In_ sai_object_id_t tunnel_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_tunnel_stat_t *counter_ids,
	_In_ sai_stats_mode_t mode,
	_Out_ uint64_t *counters)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t vs_clear_tunnel_stats(
        _In_ sai_object_id_t tunnel_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_tunnel_stat_t *counter_ids)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}


VS_GENERIC_QUAD(TUNNEL_MAP,tunnel_map);
VS_GENERIC_QUAD(TUNNEL,tunnel);
VS_GENERIC_QUAD(TUNNEL_TERM_TABLE_ENTRY,tunnel_term_table_entry);
VS_GENERIC_QUAD(TUNNEL_MAP_ENTRY,tunnel_map_entry);


const sai_tunnel_api_t vs_tunnel_api = {
    VS_GENERIC_QUAD_API(tunnel_map)
    VS_GENERIC_QUAD_API(tunnel)

    vs_get_tunnel_stats,
    vs_get_tunnel_stats_ext,
    vs_clear_tunnel_stats,

    VS_GENERIC_QUAD_API(tunnel_term_table_entry)
    VS_GENERIC_QUAD_API(tunnel_map_entry)
};
