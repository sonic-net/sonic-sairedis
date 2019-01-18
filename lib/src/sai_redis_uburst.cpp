#include "sai_redis.h"
#include "sai_redis_internal.h"

REDIS_GENERIC_QUAD(TAM_MICROBURST,tam_microburst);
REDIS_GENERIC_STATS(TAM_MICROBURST,tam_microburst);
REDIS_GENERIC_QUAD(TAM_HISTOGRAM,tam_histogram);

const sai_uburst_api_t redis_uburst_api = {

    REDIS_GENERIC_QUAD_API(tam_microburst)
    REDIS_GENERIC_STATS_API(tam_microburst)
    REDIS_GENERIC_QUAD_API(tam_histogram)
};
