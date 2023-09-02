#include "sai_redis.h"

REDIS_GENERIC_QUAD(TWAMP_SESSION,twamp_session);
REDIS_GENERIC_STATS(TWAMP_SESSION,twamp_session);

const sai_twamp_api_t redis_twamp_api = {
    REDIS_GENERIC_QUAD_API(twamp_session)
    REDIS_GENERIC_STATS_API(twamp_session)
};
