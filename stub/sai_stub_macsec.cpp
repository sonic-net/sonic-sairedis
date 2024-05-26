#include "sai_stub.h"

STUB_GENERIC_QUAD(MACSEC,macsec);
STUB_GENERIC_QUAD(MACSEC_PORT,macsec_port);
STUB_GENERIC_STATS(MACSEC_PORT,macsec_port);
STUB_GENERIC_QUAD(MACSEC_FLOW,macsec_flow);
STUB_GENERIC_STATS(MACSEC_FLOW,macsec_flow);
STUB_GENERIC_QUAD(MACSEC_SC,macsec_sc);
STUB_GENERIC_STATS(MACSEC_SC,macsec_sc);
STUB_GENERIC_QUAD(MACSEC_SA,macsec_sa);
STUB_GENERIC_STATS(MACSEC_SA,macsec_sa);

const sai_macsec_api_t stub_macsec_api = {

    STUB_GENERIC_QUAD_API(macsec)
    STUB_GENERIC_QUAD_API(macsec_port)
    STUB_GENERIC_STATS_API(macsec_port)
    STUB_GENERIC_QUAD_API(macsec_flow)
    STUB_GENERIC_STATS_API(macsec_flow)
    STUB_GENERIC_QUAD_API(macsec_sc)
    STUB_GENERIC_STATS_API(macsec_sc)
    STUB_GENERIC_QUAD_API(macsec_sa)
    STUB_GENERIC_STATS_API(macsec_sa)
};
