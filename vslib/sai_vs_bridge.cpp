#include "sai_vs.h"

VS_GENERIC_QUAD(BRIDGE,bridge);
VS_GENERIC_QUAD(BRIDGE_PORT,bridge_port);
VS_GENERIC_STATS(BRIDGE,bridge);
VS_GENERIC_STATS(BRIDGE_PORT,bridge_port);

const sai_bridge_api_t vs_bridge_api = {

    VS_GENERIC_QUAD_API(bridge)
    VS_GENERIC_STATS_API(bridge)
    VS_GENERIC_QUAD_API(bridge_port)
    VS_GENERIC_STATS_API(bridge_port)
};
