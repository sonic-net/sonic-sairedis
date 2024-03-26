#include "sai_proxy.h"

PROXY_GENERIC_QUAD(ROUTER_INTERFACE,router_interface);
PROXY_GENERIC_STATS(ROUTER_INTERFACE,router_interface);
PROXY_BULK_QUAD(ROUTER_INTERFACE,router_interfaces);

const sai_router_interface_api_t proxy_router_interface_api = {

    PROXY_GENERIC_QUAD_API(router_interface)
    PROXY_GENERIC_STATS_API(router_interface)
    PROXY_BULK_QUAD_API(router_interfaces)
};
