#include "sai_stub.h"

STUB_GENERIC_QUAD(ROUTER_INTERFACE,router_interface);
STUB_GENERIC_STATS(ROUTER_INTERFACE,router_interface);
STUB_BULK_QUAD(ROUTER_INTERFACE,router_interfaces);

const sai_router_interface_api_t stub_router_interface_api = {

    STUB_GENERIC_QUAD_API(router_interface)
    STUB_GENERIC_STATS_API(router_interface)
    STUB_BULK_QUAD_API(router_interfaces)
};
