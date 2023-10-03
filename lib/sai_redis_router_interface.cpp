#include "sai_redis.h"

REDIS_GENERIC_QUAD(ROUTER_INTERFACE,router_interface);
REDIS_GENERIC_STATS(ROUTER_INTERFACE,router_interface);
REDIS_BULK_QUAD(ROUTER_INTERFACE,router_interfaces);

const sai_router_interface_api_t redis_router_interface_api = {

    REDIS_GENERIC_QUAD_API(router_interface)
    REDIS_GENERIC_STATS_API(router_interface)
    REDIS_BULK_QUAD_API(router_interfaces)
};
