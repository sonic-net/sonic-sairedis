#include "sai_redis.h"

REDIS_GENERIC_QUAD(IPSEC,ipsec);
REDIS_GENERIC_QUAD(IPSEC_PORT,ipsec_port);
REDIS_GENERIC_QUAD(IPSEC_SA,ipsec_sa);
REDIS_GENERIC_STATS(IPSEC_PORT,ipsec_port);
REDIS_GENERIC_STATS(IPSEC_SA,ipsec_sa);

const sai_ipsec_api_t redis_ipsec_api = {

    REDIS_GENERIC_QUAD_API(ipsec)
    REDIS_GENERIC_QUAD_API(ipsec_port)
    REDIS_GENERIC_STATS_API(ipsec_port)
    REDIS_GENERIC_QUAD_API(ipsec_sa)
    REDIS_GENERIC_STATS_API(ipsec_sa)
};
