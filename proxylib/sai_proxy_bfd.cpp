#include "sai_proxy.h"

PROXY_GENERIC_QUAD(BFD_SESSION,bfd_session);
PROXY_GENERIC_STATS(BFD_SESSION,bfd_session);

const sai_bfd_api_t proxy_bfd_api = {

    PROXY_GENERIC_QUAD_API(bfd_session)
    PROXY_GENERIC_STATS_API(bfd_session)
};
