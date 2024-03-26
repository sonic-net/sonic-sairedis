#include "sai_proxy.h"

PROXY_GENERIC_QUAD(SAMPLEPACKET,samplepacket);

const sai_samplepacket_api_t proxy_samplepacket_api = {

    PROXY_GENERIC_QUAD_API(samplepacket)
};
