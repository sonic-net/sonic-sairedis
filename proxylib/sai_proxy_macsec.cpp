#include "sai_proxy.h"

PROXY_GENERIC_QUAD(MACSEC,macsec);
PROXY_GENERIC_QUAD(MACSEC_PORT,macsec_port);
PROXY_GENERIC_STATS(MACSEC_PORT,macsec_port);
PROXY_GENERIC_QUAD(MACSEC_FLOW,macsec_flow);
PROXY_GENERIC_STATS(MACSEC_FLOW,macsec_flow);
PROXY_GENERIC_QUAD(MACSEC_SC,macsec_sc);
PROXY_GENERIC_STATS(MACSEC_SC,macsec_sc);
PROXY_GENERIC_QUAD(MACSEC_SA,macsec_sa);
PROXY_GENERIC_STATS(MACSEC_SA,macsec_sa);

const sai_macsec_api_t proxy_macsec_api = {

    PROXY_GENERIC_QUAD_API(macsec)
    PROXY_GENERIC_QUAD_API(macsec_port)
    PROXY_GENERIC_STATS_API(macsec_port)
    PROXY_GENERIC_QUAD_API(macsec_flow)
    PROXY_GENERIC_STATS_API(macsec_flow)
    PROXY_GENERIC_QUAD_API(macsec_sc)
    PROXY_GENERIC_STATS_API(macsec_sc)
    PROXY_GENERIC_QUAD_API(macsec_sa)
    PROXY_GENERIC_STATS_API(macsec_sa)
};
