#include "sai_vs.h"
#include "sai_vs_internal.h"

sai_status_t vs_get_tam_snapshot_stats(
        _In_ sai_object_id_t tam_snapshot_id,
        _Inout_ uint32_t *number_of_counters,
        _Inout_ sai_tam_statistic_t *statistics)
{
    MUTEX();
    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

VS_GENERIC_QUAD(TAM,tam);
VS_GENERIC_QUAD(TAM_STAT,tam_stat);
VS_GENERIC_QUAD(TAM_THRESHOLD,tam_threshold);
VS_GENERIC_QUAD(TAM_SNAPSHOT,tam_snapshot);
VS_GENERIC_QUAD(TAM_TRANSPORTER,tam_transporter);

const sai_tam_api_t vs_tam_api = {

    VS_GENERIC_QUAD_API(tam)
    VS_GENERIC_QUAD_API(tam_stat)
    VS_GENERIC_QUAD_API(tam_threshold)
    VS_GENERIC_QUAD_API(tam_snapshot)

    vs_get_tam_snapshot_stats,

    VS_GENERIC_QUAD_API(tam_transporter)
};
