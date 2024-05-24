#include "sai_proxy.h"

PROXY_GENERIC_QUAD(POE_DEVICE,poe_device);
PROXY_GENERIC_QUAD(POE_PSE,poe_pse);
PROXY_GENERIC_QUAD(POE_PORT,poe_port);

const sai_poe_api_t proxy_poe_api = {

    PROXY_GENERIC_QUAD_API(poe_device)
    PROXY_GENERIC_QUAD_API(poe_pse)
    PROXY_GENERIC_QUAD_API(poe_port)
};
