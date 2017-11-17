#include "sai_redis.h"

REDIS_GENERIC_QUAD(SCHEDULER,scheduler);

const sai_scheduler_api_t redis_scheduler_api = {

    REDIS_GENERIC_QUAD_API(scheduler)
};
