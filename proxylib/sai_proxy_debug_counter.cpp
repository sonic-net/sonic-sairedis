#include "sai_proxy.h"

PROXY_GENERIC_QUAD(DEBUG_COUNTER,debug_counter);

const sai_debug_counter_api_t proxy_debug_counter_api = {

    PROXY_GENERIC_QUAD_API(debug_counter)
};
