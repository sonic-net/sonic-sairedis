#include "sai_redis.h"

REDIS_BULK_CREATE(LAG_MEMBER,lag_members);
REDIS_BULK_REMOVE(LAG_MEMBER,lag_members);

REDIS_GENERIC_QUAD(LAG,lag);
REDIS_GENERIC_QUAD(LAG_MEMBER,lag_member);

const sai_lag_api_t redis_lag_api = {

    REDIS_GENERIC_QUAD_API(lag)
    REDIS_GENERIC_QUAD_API(lag_member)

    redis_bulk_create_lag_members,
    redis_bulk_remove_lag_members,
};
