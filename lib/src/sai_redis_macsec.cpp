#include "sai_redis.h"

REDIS_GENERIC_QUAD(MACSEC,macsec);

REDIS_GENERIC_QUAD(MACSEC_PORT,macsec_port);
REDIS_GENERIC_STATS(MACSEC_PORT,macsec_port);

REDIS_GENERIC_QUAD(MACSEC_FLOW,macsec_flow);
REDIS_GENERIC_STATS(MACSEC_FLOW,macsec_flow);

REDIS_GENERIC_QUAD(MACSEC_SC,macsec_sc);
REDIS_GENERIC_STATS(MACSEC_SC,macsec_sc);

REDIS_GENERIC_QUAD(MACSEC_SA,macsec_sa);
REDIS_GENERIC_STATS(MACSEC_SA,macsec_sa);

const sai_macsec_api_t redis_macsec_api = {

    REDIS_GENERIC_QUAD_API(macsec)

    REDIS_GENERIC_QUAD_API(macsec_port)
    REDIS_GENERIC_STATS_API(macsec_port)

    REDIS_GENERIC_QUAD_API(macsec_flow)
    REDIS_GENERIC_STATS_API(macsec_flow)

    REDIS_GENERIC_QUAD_API(macsec_sc)
    REDIS_GENERIC_STATS_API(macsec_sc)

    REDIS_GENERIC_QUAD_API(macsec_sa)
    REDIS_GENERIC_STATS_API(macsec_sa)
};
