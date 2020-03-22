#include "sai_vs.h"
#include "sai_vs_internal.h"

VS_GENERIC_QUAD(VIRTUAL_ROUTER,virtual_router);

const sai_virtual_router_api_t vs_virtual_router_api = {

    VS_GENERIC_QUAD_API(virtual_router)
};
