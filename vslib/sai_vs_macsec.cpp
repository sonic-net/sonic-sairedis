#include "sai_vs.h"

VS_GENERIC_QUAD(MACSEC,macsec);

VS_GENERIC_QUAD(MACSEC_PORT,macsec_port);
VS_GENERIC_STATS(MACSEC_PORT,macsec_port);

VS_GENERIC_QUAD(MACSEC_FLOW,macsec_flow);
VS_GENERIC_STATS(MACSEC_FLOW,macsec_flow);

VS_GENERIC_QUAD(MACSEC_SC,macsec_sc);
VS_GENERIC_STATS(MACSEC_SC,macsec_sc);

VS_GENERIC_QUAD(MACSEC_SA,macsec_sa);
VS_GENERIC_STATS(MACSEC_SA,macsec_sa);

const sai_macsec_api_t vs_macsec_api = {

    VS_GENERIC_QUAD_API(macsec)

    VS_GENERIC_QUAD_API(macsec_port)
    VS_GENERIC_STATS_API(macsec_port)

    VS_GENERIC_QUAD_API(macsec_flow)
    VS_GENERIC_STATS_API(macsec_flow)

    VS_GENERIC_QUAD_API(macsec_sc)
    VS_GENERIC_STATS_API(macsec_sc)

    VS_GENERIC_QUAD_API(macsec_sa)
    VS_GENERIC_STATS_API(macsec_sa)
};
