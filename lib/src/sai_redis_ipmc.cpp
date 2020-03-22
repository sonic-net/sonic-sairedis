#include "sai_redis.h"
#include "sai_redis_internal.h"

REDIS_GENERIC_QUAD_ENTRY(IPMC_ENTRY,ipmc_entry);

const sai_ipmc_api_t redis_ipmc_api = {

    REDIS_GENERIC_QUAD_API(ipmc_entry)
};
