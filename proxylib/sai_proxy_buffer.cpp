#include "sai_proxy.h"

PROXY_GENERIC_QUAD(BUFFER_POOL,buffer_pool);
PROXY_GENERIC_QUAD(INGRESS_PRIORITY_GROUP,ingress_priority_group);
PROXY_GENERIC_QUAD(BUFFER_PROFILE,buffer_profile);
PROXY_GENERIC_STATS(BUFFER_POOL,buffer_pool);
PROXY_GENERIC_STATS(INGRESS_PRIORITY_GROUP,ingress_priority_group);

const sai_buffer_api_t proxy_buffer_api = {

    PROXY_GENERIC_QUAD_API(buffer_pool)
    PROXY_GENERIC_STATS_API(buffer_pool)
    PROXY_GENERIC_QUAD_API(ingress_priority_group)
    PROXY_GENERIC_STATS_API(ingress_priority_group)
    PROXY_GENERIC_QUAD_API(buffer_profile)
};
