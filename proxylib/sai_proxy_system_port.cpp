#include "sai_proxy.h"

PROXY_GENERIC_QUAD(SYSTEM_PORT,system_port);

const sai_system_port_api_t proxy_system_port_api = {

    PROXY_GENERIC_QUAD_API(system_port)
};
