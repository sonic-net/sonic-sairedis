#include "sai_redis.h"

REDIS_GENERIC_QUAD(NEXT_HOP,next_hop);

const sai_next_hop_api_t redis_next_hop_api = {

    REDIS_GENERIC_QUAD_API(next_hop)
};
