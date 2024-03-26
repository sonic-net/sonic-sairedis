#include "sai_proxy.h"

PROXY_GENERIC_QUAD(BRIDGE,bridge);
PROXY_GENERIC_QUAD(BRIDGE_PORT,bridge_port);
PROXY_GENERIC_STATS(BRIDGE,bridge);
PROXY_GENERIC_STATS(BRIDGE_PORT,bridge_port);

const sai_bridge_api_t proxy_bridge_api = {

    PROXY_GENERIC_QUAD_API(bridge)
    PROXY_GENERIC_STATS_API(bridge)
    PROXY_GENERIC_QUAD_API(bridge_port)
    PROXY_GENERIC_STATS_API(bridge_port)
};
