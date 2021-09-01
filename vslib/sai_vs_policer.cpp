#include "sai_vs.h"

VS_GENERIC_QUAD(POLICER,policer);
VS_GENERIC_STATS(POLICER,policer);

const sai_policer_api_t vs_policer_api = {

    VS_GENERIC_QUAD_API(policer)
    VS_GENERIC_STATS_API(policer)
};
