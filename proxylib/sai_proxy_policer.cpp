#include "sai_proxy.h"

PROXY_GENERIC_QUAD(POLICER,policer);
PROXY_GENERIC_STATS(POLICER,policer);

const sai_policer_api_t proxy_policer_api = {

    PROXY_GENERIC_QUAD_API(policer)
    PROXY_GENERIC_STATS_API(policer)
};
