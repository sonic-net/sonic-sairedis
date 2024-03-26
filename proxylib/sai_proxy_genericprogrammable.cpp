#include "sai_proxy.h"

PROXY_GENERIC_QUAD(GENERIC_PROGRAMMABLE,generic_programmable);

const sai_generic_programmable_api_t proxy_generic_programmable_api = {
    PROXY_GENERIC_QUAD_API(generic_programmable)
};
