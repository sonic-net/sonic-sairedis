#include "sai_proxy.h"

PROXY_GENERIC_QUAD(NEXT_HOP,next_hop);
PROXY_BULK_QUAD(NEXT_HOP,next_hop);

const sai_next_hop_api_t proxy_next_hop_api = {

    PROXY_GENERIC_QUAD_API(next_hop)
    PROXY_BULK_QUAD_API(next_hop)
};
