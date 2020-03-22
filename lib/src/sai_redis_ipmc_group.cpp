#include "sai_redis.h"
#include "sai_redis_internal.h"

REDIS_GENERIC_QUAD(IPMC_GROUP,ipmc_group);
REDIS_GENERIC_QUAD(IPMC_GROUP_MEMBER,ipmc_group_member);

const sai_ipmc_group_api_t redis_ipmc_group_api = {

    REDIS_GENERIC_QUAD_API(ipmc_group)
    REDIS_GENERIC_QUAD_API(ipmc_group_member)
};
