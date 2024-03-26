#include "sai_proxy.h"

PROXY_GENERIC_QUAD(COUNTER,counter);
PROXY_GENERIC_STATS(COUNTER,counter);

const sai_counter_api_t proxy_counter_api = {

    PROXY_GENERIC_QUAD_API(counter)
    PROXY_GENERIC_STATS_API(counter)
};
