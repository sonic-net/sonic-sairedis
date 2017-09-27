#include "sai_redis.h"

REDIS_GENERIC_QUAD(ROUTER_INTERFACE,router_interface);

const sai_router_interface_api_t redis_router_interface_api = {

    REDIS_GENERIC_QUAD_API(router_interface)
};
