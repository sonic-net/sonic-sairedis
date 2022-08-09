#include "sai_vs.h"

VS_GENERIC_QUAD(COUNTER,counter);
VS_GENERIC_STATS(COUNTER,counter);

const sai_counter_api_t vs_counter_api = {

    VS_GENERIC_QUAD_API(counter)
    VS_GENERIC_STATS_API(counter)
};

