#include "sai_redis.h"

REDIS_GENERIC_QUAD(ARS, ars);

const sai_ars_api_t redis_ars_api = {
    REDIS_GENERIC_QUAD_API(ars)
};
