#include "sai_redis.h"

REDIS_GENERIC_QUAD(VNET, vnet);
REDIS_BULK_CREATE(VNET, vnets);
REDIS_BULK_REMOVE(VNET, vnets);

const sai_dash_vnet_api_t redis_dash_vnet_api = {
    REDIS_GENERIC_QUAD_API(vnet)
    redis_bulk_create_vnets,
    redis_bulk_remove_vnets,
};
