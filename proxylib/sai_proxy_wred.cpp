#include "sai_proxy.h"

PROXY_GENERIC_QUAD(WRED,wred);

const sai_wred_api_t proxy_wred_api = {

    PROXY_GENERIC_QUAD_API(wred)
};
