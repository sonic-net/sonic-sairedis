#include "sai_vs.h"
#include "sai_vs_internal.h"

VS_GENERIC_QUAD(ROUTER_INTERFACE,router_interface);

const sai_router_interface_api_t vs_router_interface_api = {

    VS_GENERIC_QUAD_API(router_interface)
};
