#include "sai_redis.h"

REDIS_GENERIC_QUAD(WRED,wred);

const sai_wred_api_t redis_wred_api = {

    REDIS_GENERIC_QUAD_API(wred)
};
