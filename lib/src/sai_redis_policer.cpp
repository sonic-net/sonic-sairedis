#include "sai_redis.h"

REDIS_GENERIC_QUAD(POLICER,policer);
REDIS_GENERIC_STATS(POLICER,policer);

const sai_policer_api_t redis_policer_api = {

    REDIS_GENERIC_QUAD_API(policer)
    REDIS_GENERIC_STATS_API(policer)
};
