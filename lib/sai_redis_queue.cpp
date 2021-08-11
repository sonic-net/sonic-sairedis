#include "sai_redis.h"

REDIS_GENERIC_QUAD(QUEUE,queue);
REDIS_GENERIC_STATS(QUEUE,queue);

const sai_queue_api_t redis_queue_api = {

    REDIS_GENERIC_QUAD_API(queue)
    REDIS_GENERIC_STATS_API(queue)
};
