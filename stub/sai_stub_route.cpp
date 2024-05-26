#include "sai_stub.h"

STUB_GENERIC_QUAD_ENTRY(ROUTE_ENTRY,route_entry);
STUB_BULK_QUAD_ENTRY(ROUTE_ENTRY,route_entry);

const sai_route_api_t stub_route_api = {

    STUB_GENERIC_QUAD_API(route_entry)
    STUB_BULK_QUAD_API(route_entry)
};
