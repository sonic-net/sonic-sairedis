#include "sai_redis.h"


REDIS_GENERIC_QUAD(ARS_PROFILE, ars_profile);

const sai_ars_profile_api_t redis_ars_profile_api = {
    REDIS_GENERIC_QUAD_API(ars_profile)
};
