#include "sai_vs.h"

VS_GENERIC_QUAD(VNET, vnet);
VS_BULK_CREATE(VNET, vnets);
VS_BULK_REMOVE(VNET, vnets);

const sai_dash_vnet_api_t vs_dash_vnet_api = {
    VS_GENERIC_QUAD_API(vnet)
    vs_bulk_create_vnets,
    vs_bulk_remove_vnets,
};
