#include "sai_stub.h"

STUB_GENERIC_QUAD(BRIDGE,bridge);
STUB_GENERIC_QUAD(BRIDGE_PORT,bridge_port);
STUB_GENERIC_STATS(BRIDGE,bridge);
STUB_GENERIC_STATS(BRIDGE_PORT,bridge_port);

const sai_bridge_api_t stub_bridge_api = {

    STUB_GENERIC_QUAD_API(bridge)
    STUB_GENERIC_STATS_API(bridge)
    STUB_GENERIC_QUAD_API(bridge_port)
    STUB_GENERIC_STATS_API(bridge_port)
};
