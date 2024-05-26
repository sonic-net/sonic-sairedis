#include "sai_stub.h"

STUB_GENERIC_QUAD(IPSEC,ipsec);
STUB_GENERIC_QUAD(IPSEC_PORT,ipsec_port);
STUB_GENERIC_QUAD(IPSEC_SA,ipsec_sa);
STUB_GENERIC_STATS(IPSEC_PORT,ipsec_port);
STUB_GENERIC_STATS(IPSEC_SA,ipsec_sa);

const sai_ipsec_api_t stub_ipsec_api = {

    STUB_GENERIC_QUAD_API(ipsec)
    STUB_GENERIC_QUAD_API(ipsec_port)
    STUB_GENERIC_STATS_API(ipsec_port)
    STUB_GENERIC_QUAD_API(ipsec_sa)
    STUB_GENERIC_STATS_API(ipsec_sa)
};
