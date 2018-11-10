#include "sai_redis.h"
#include "sai_redis_internal.h"

REDIS_GENERIC_QUAD_ENTRY(L2MC_ENTRY,l2mc_entry);

const sai_l2mc_api_t redis_l2mc_api = {

    REDIS_GENERIC_QUAD_API(l2mc_entry)
};
