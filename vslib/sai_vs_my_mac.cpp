#include "sai_vs.h"

VS_GENERIC_QUAD(IPSEC,my_mac);

const sai_my_mac_api_t vs_my_mac_api = {

    VS_GENERIC_QUAD_API(my_mac)
};
