#include "sai_proxy.h"

PROXY_GENERIC_QUAD(TWAMP_SESSION,twamp_session);
PROXY_GENERIC_STATS(TWAMP_SESSION,twamp_session);

const sai_twamp_api_t proxy_twamp_api = {
    PROXY_GENERIC_QUAD_API(twamp_session)
    PROXY_GENERIC_STATS_API(twamp_session)
};
