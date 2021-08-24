#include "sai_redis.h"

REDIS_GENERIC_QUAD(COUNTER,counter);
REDIS_GENERIC_STATS(COUNTER,counter);

const sai_counter_api_t redis_counter_api = {

    REDIS_GENERIC_QUAD_API(counter)
    REDIS_GENERIC_STATS_API(counter)
};
