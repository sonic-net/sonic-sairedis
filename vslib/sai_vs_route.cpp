#include "sai_vs.h"

VS_GENERIC_QUAD_ENTRY(ROUTE_ENTRY,route_entry);

VS_BULK_QUAD_ENTRY(ROUTE_ENTRY,route_entry);

const sai_route_api_t vs_route_api = {

    VS_GENERIC_QUAD_API(route_entry)
    VS_BULK_QUAD_API(route_entry)
};
