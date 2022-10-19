#include "sai_redis.h"

REDIS_GENERIC_QUAD(GENERIC_PROGRAMMABLE,generic_programmable);

const sai_generic_programmable_api_t redis_generic_programmable_api = {
    REDIS_GENERIC_QUAD_API(generic_programmable)
};
