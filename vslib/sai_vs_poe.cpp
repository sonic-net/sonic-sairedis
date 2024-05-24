#include "sai_vs.h"

VS_GENERIC_QUAD(POE_DEVICE,poe_device);
VS_GENERIC_QUAD(POE_PSE,poe_pse);
VS_GENERIC_QUAD(POE_PORT,poe_port);

const sai_poe_api_t vs_poe_api = {

    VS_GENERIC_QUAD_API(poe_device)
    VS_GENERIC_QUAD_API(poe_pse)
    VS_GENERIC_QUAD_API(poe_port)
};
