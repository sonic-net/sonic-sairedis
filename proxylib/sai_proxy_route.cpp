#include "sai_proxy.h"

PROXY_GENERIC_QUAD_ENTRY(ROUTE_ENTRY,route_entry);
PROXY_BULK_QUAD_ENTRY(ROUTE_ENTRY,route_entry);

const sai_route_api_t proxy_route_api = {

    PROXY_GENERIC_QUAD_API(route_entry)
    PROXY_BULK_QUAD_API(route_entry)
};
