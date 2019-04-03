#include "sai_vs.h"
#include "sai_vs_internal.h"

VS_GENERIC_QUAD(TAM_MICROBURST,tam_microburst);
VS_GENERIC_STATS(TAM_MICROBURST,tam_microburst);
VS_GENERIC_QUAD(TAM_HISTOGRAM,tam_histogram);

const sai_uburst_api_t vs_uburst_api = {

    VS_GENERIC_QUAD_API(tam_microburst)
    VS_GENERIC_STATS_API(tam_microburst)
    VS_GENERIC_QUAD_API(tam_histogram)
};
