#include "sai_redis.h"

REDIS_GENERIC_QUAD(DEBUG_COUNTER,debug_counter);

const sai_debug_counter_api_t redis_debug_counter_api = {

    REDIS_GENERIC_QUAD_API(debug_counter)
};
