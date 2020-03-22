#include "sai_redis.h"
#include "sai_redis_internal.h"

REDIS_GENERIC_QUAD(L2MC_GROUP,l2mc_group);
REDIS_GENERIC_QUAD(L2MC_GROUP_MEMBER,l2mc_group_member);

const sai_l2mc_group_api_t redis_l2mc_group_api = {

    REDIS_GENERIC_QUAD_API(l2mc_group)
    REDIS_GENERIC_QUAD_API(l2mc_group_member)
};
