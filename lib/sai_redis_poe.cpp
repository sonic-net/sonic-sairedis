#include "sai_redis.h"

REDIS_GENERIC_QUAD(POE_DEVICE,poe_device);
REDIS_GENERIC_QUAD(POE_PSE,poe_pse);
REDIS_GENERIC_QUAD(POE_PORT,poe_port);

const sai_poe_api_t redis_poe_api = {

    REDIS_GENERIC_QUAD_API(poe_device)
    REDIS_GENERIC_QUAD_API(poe_pse)
    REDIS_GENERIC_QUAD_API(poe_port)
};
