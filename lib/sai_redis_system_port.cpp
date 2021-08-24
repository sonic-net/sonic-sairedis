#include "sai_redis.h"

REDIS_GENERIC_QUAD(SYSTEM_PORT,system_port);

const sai_system_port_api_t redis_system_port_api = {

    REDIS_GENERIC_QUAD_API(system_port)
};
