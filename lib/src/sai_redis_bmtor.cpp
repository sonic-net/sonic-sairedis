#include "sai_redis.h"

sai_status_t redis_get_table_bitmap_classification_entry_stats(
        _In_ sai_object_id_t table_bitmap_classification_entry_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids,
        _Out_ uint64_t *counters)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t redis_get_table_bitmap_classification_entry_stats_ext(
        _In_ sai_object_id_t table_bitmap_classification_entry_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids,
        _In_ sai_stats_mode_t mode,
        _Out_ uint64_t *counters)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t redis_clear_table_bitmap_classification_entry_stats(
        _In_ sai_object_id_t table_bitmap_classification_entry_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

REDIS_GENERIC_QUAD(TABLE_BITMAP_CLASSIFICATION_ENTRY,table_bitmap_classification_entry);

sai_status_t redis_get_table_bitmap_router_entry_stats(
        _In_ sai_object_id_t table_bitmap_router_entry_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids,
        _Out_ uint64_t *counters)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t redis_get_table_bitmap_router_entry_stats_ext(
        _In_ sai_object_id_t table_bitmap_router_entry_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids,
        _In_ sai_stats_mode_t mode,
        _Out_ uint64_t *counters)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

sai_status_t redis_clear_table_bitmap_router_entry_stats(
        _In_ sai_object_id_t table_bitmap_router_entry_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

REDIS_GENERIC_QUAD(TABLE_BITMAP_ROUTER_ENTRY,table_bitmap_router_entry);

const sai_bmtor_api_t redis_bmtor_api = {

    REDIS_GENERIC_QUAD_API(table_bitmap_classification_entry)

    redis_get_table_bitmap_classification_entry_stats,
    redis_get_table_bitmap_classification_entry_stats_ext,
    redis_clear_table_bitmap_classification_entry_stats,

    REDIS_GENERIC_QUAD_API(table_bitmap_router_entry)

    redis_get_table_bitmap_router_entry_stats,
    redis_get_table_bitmap_router_entry_stats_ext,
    redis_clear_table_bitmap_router_entry_stats,
};
