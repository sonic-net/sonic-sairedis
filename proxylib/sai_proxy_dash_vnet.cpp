#include "sai_proxy.h"

PROXY_GENERIC_QUAD(VNET, vnet);
PROXY_BULK_CREATE(VNET, vnets);
PROXY_BULK_REMOVE(VNET, vnets);

const sai_dash_vnet_api_t proxy_dash_vnet_api = {
    PROXY_GENERIC_QUAD_API(vnet)
    proxy_bulk_create_vnets,
    proxy_bulk_remove_vnets,
};
