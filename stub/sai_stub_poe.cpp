#include "sai_stub.h"

STUB_GENERIC_QUAD(POE_DEVICE,poe_device);
STUB_GENERIC_QUAD(POE_PSE,poe_pse);
STUB_GENERIC_QUAD(POE_PORT,poe_port);

const sai_poe_api_t stub_poe_api = {

    STUB_GENERIC_QUAD_API(poe_device)
    STUB_GENERIC_QUAD_API(poe_pse)
    STUB_GENERIC_QUAD_API(poe_port)
};
