#include "sai_redis.h"

REDIS_GENERIC_QUAD(SCHEDULER_GROUP,scheduler_group);

const sai_scheduler_group_api_t redis_scheduler_group_api = {

    REDIS_GENERIC_QUAD_API(scheduler_group)
};
