#include "sai_proxy.h"

PROXY_GENERIC_QUAD(MY_MAC,my_mac);

const sai_my_mac_api_t proxy_my_mac_api = {

    PROXY_GENERIC_QUAD_API(my_mac)
};
