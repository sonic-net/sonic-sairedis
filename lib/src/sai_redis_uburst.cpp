#include "sai_redis.h"
#include "sai_redis_internal.h"

REDIS_GENERIC_QUAD(TAM_MICROBURST,tam_microburst);
REDIS_GENERIC_QUAD(TAM_HISTOGRAM,tam_histogram);

sai_status_t redis_get_tam_histogram_stats(
        _In_ sai_object_id_t tam_histogram_id,
        _Inout_ uint32_t *number_of_counters,
        _Out_ uint64_t *counters)
{
    MUTEX();

    SWSS_LOG_ENTER();

    return SAI_STATUS_NOT_IMPLEMENTED;
}

const sai_uburst_api_t redis_uburst_api = {

    REDIS_GENERIC_QUAD_API(tam_microburst)
    REDIS_GENERIC_QUAD_API(tam_histogram)
    
    redis_get_tam_histogram_stats
};
