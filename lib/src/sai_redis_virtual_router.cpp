#include "sai_redis.h"

REDIS_GENERIC_QUAD(VIRTUAL_ROUTER,virtual_router);

const sai_virtual_router_api_t redis_virtual_router_api = {

    REDIS_GENERIC_QUAD_API(virtual_router)
};
