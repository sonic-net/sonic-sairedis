#include "sai_redis.h"

REDIS_GENERIC_QUAD(BRIDGE,bridge);
REDIS_GENERIC_QUAD(BRIDGE_PORT,bridge_port);
REDIS_GENERIC_STATS(BRIDGE,bridge);
REDIS_GENERIC_STATS(BRIDGE_PORT,bridge_port);

const sai_bridge_api_t redis_bridge_api = {

    REDIS_GENERIC_QUAD_API(bridge)
    REDIS_GENERIC_STATS_API(bridge)
    REDIS_GENERIC_QUAD_API(bridge_port)
    REDIS_GENERIC_STATS_API(bridge_port)
};
