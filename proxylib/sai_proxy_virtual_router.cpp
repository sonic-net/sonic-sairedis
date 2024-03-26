#include "sai_proxy.h"

PROXY_GENERIC_QUAD(VIRTUAL_ROUTER,virtual_router);

const sai_virtual_router_api_t proxy_virtual_router_api = {

    PROXY_GENERIC_QUAD_API(virtual_router)
};
