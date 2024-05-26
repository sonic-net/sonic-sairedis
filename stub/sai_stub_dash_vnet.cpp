#include "sai_stub.h"

STUB_GENERIC_QUAD(VNET, vnet);
STUB_BULK_CREATE(VNET, vnets);
STUB_BULK_REMOVE(VNET, vnets);

const sai_dash_vnet_api_t stub_dash_vnet_api = {
    STUB_GENERIC_QUAD_API(vnet)
    stub_bulk_create_vnets,
    stub_bulk_remove_vnets,
};
