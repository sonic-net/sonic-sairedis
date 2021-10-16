#include "sai_redis.h"

REDIS_GENERIC_QUAD_ENTRY(ROUTE_ENTRY,route_entry);
REDIS_BULK_QUAD_ENTRY(ROUTE_ENTRY,route_entry);

const sai_route_api_t redis_route_api = {

    REDIS_GENERIC_QUAD_API(route_entry)
    REDIS_BULK_QUAD_API(route_entry)
};
