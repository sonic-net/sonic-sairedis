#include "sai_vs.h"

VS_GENERIC_QUAD(IPSEC,ipsec);
VS_GENERIC_QUAD(IPSEC_PORT,ipsec_port);
VS_GENERIC_QUAD(IPSEC_SA,ipsec_sa);
VS_GENERIC_STATS(IPSEC_PORT,ipsec_port);
VS_GENERIC_STATS(IPSEC_SA,ipsec_sa);

const sai_ipsec_api_t vs_ipsec_api = {

    VS_GENERIC_QUAD_API(ipsec)
    VS_GENERIC_QUAD_API(ipsec_port)
    VS_GENERIC_STATS_API(ipsec_port)
    VS_GENERIC_QUAD_API(ipsec_sa)
    VS_GENERIC_STATS_API(ipsec_sa)
};
