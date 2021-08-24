#include "sai_redis.h"

REDIS_GENERIC_QUAD(RPF_GROUP,rpf_group);
REDIS_GENERIC_QUAD(RPF_GROUP_MEMBER,rpf_group_member);

const sai_rpf_group_api_t redis_rpf_group_api = {

    REDIS_GENERIC_QUAD_API(rpf_group)
    REDIS_GENERIC_QUAD_API(rpf_group_member)
};
