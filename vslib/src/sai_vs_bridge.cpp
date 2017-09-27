#include "sai_vs.h"
#include "sai_vs_internal.h"

VS_GENERIC_QUAD(BRIDGE,bridge);
VS_GENERIC_QUAD(BRIDGE_PORT,bridge_port);

const sai_bridge_api_t vs_bridge_api =
{
    VS_GENERIC_QUAD_API(bridge)
    VS_GENERIC_QUAD_API(bridge_port)
};
