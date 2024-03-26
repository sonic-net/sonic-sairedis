#include "sai_proxy.h"

PROXY_GENERIC_QUAD(ARS, ars);

const sai_ars_api_t proxy_ars_api = {
    PROXY_GENERIC_QUAD_API(ars)
};
