#include "sai_redis.h"

REDIS_GENERIC_QUAD(MIRROR_SESSION,mirror_session);

const sai_mirror_api_t redis_mirror_api = {

    REDIS_GENERIC_QUAD_API(mirror_session)
};
