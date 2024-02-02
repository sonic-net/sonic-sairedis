#include "sai_vs.h"

VS_GENERIC_QUAD(ROUTER_INTERFACE,router_interface);
VS_GENERIC_STATS(ROUTER_INTERFACE,router_interface);

const sai_router_interface_api_t vs_router_interface_api = {

    VS_GENERIC_QUAD_API(router_interface)
    VS_GENERIC_STATS_API(router_interface)
};
