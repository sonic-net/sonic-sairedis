#include "sai_vs.h"
#include "sai_vs_internal.h"

VS_GENERIC_QUAD(TAM_MICROBURST,tam_microburst);
VS_GENERIC_QUAD(TAM_HISTOGRAM,tam_histogram);

sai_status_t vs_get_tam_histogram_stats(
        _In_ sai_object_id_t tam_histogram_id,
        _Inout_ uint32_t *number_of_counters,
        _Out_ uint64_t *counters)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

const sai_uburst_api_t vs_uburst_api = {

    VS_GENERIC_QUAD_API(tam_microburst)
    VS_GENERIC_QUAD_API(tam_histogram)
    
    vs_get_tam_histogram_stats
};
