#include "sai_proxy.h"

PROXY_GENERIC_QUAD(IPSEC,ipsec);
PROXY_GENERIC_QUAD(IPSEC_PORT,ipsec_port);
PROXY_GENERIC_QUAD(IPSEC_SA,ipsec_sa);
PROXY_GENERIC_STATS(IPSEC_PORT,ipsec_port);
PROXY_GENERIC_STATS(IPSEC_SA,ipsec_sa);

const sai_ipsec_api_t proxy_ipsec_api = {

    PROXY_GENERIC_QUAD_API(ipsec)
    PROXY_GENERIC_QUAD_API(ipsec_port)
    PROXY_GENERIC_STATS_API(ipsec_port)
    PROXY_GENERIC_QUAD_API(ipsec_sa)
    PROXY_GENERIC_STATS_API(ipsec_sa)
};
