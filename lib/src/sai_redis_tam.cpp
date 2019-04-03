#include "sai_redis.h"
#include "sai_redis_internal.h"

sai_status_t redis_get_tam_snapshot_stats(
        _In_ sai_object_id_t tam_snapshot_id,
        _Inout_ uint32_t *number_of_counters,
        _Inout_ sai_tam_statistic_t *statistics)
{
    MUTEX();
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

REDIS_GENERIC_QUAD(TAM,tam);
REDIS_GENERIC_QUAD(TAM_STAT,tam_stat);
REDIS_GENERIC_QUAD(TAM_THRESHOLD,tam_threshold);
REDIS_GENERIC_QUAD(TAM_SNAPSHOT,tam_snapshot);
REDIS_GENERIC_QUAD(TAM_TRANSPORTER,tam_transporter);

const sai_tam_api_t redis_tam_api = {

    REDIS_GENERIC_QUAD_API(tam)
    REDIS_GENERIC_QUAD_API(tam_stat)
    REDIS_GENERIC_QUAD_API(tam_threshold)
    REDIS_GENERIC_QUAD_API(tam_snapshot)

    redis_get_tam_snapshot_stats,

    REDIS_GENERIC_QUAD_API(tam_transporter)
};
