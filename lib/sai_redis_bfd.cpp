#include "sai_redis.h"

REDIS_GENERIC_QUAD(BFD_SESSION,bfd_session);
REDIS_GENERIC_STATS(BFD_SESSION,bfd_session);

const sai_bfd_api_t redis_bfd_api = {

    REDIS_GENERIC_QUAD_API(bfd_session)
    REDIS_GENERIC_STATS_API(bfd_session)
};
