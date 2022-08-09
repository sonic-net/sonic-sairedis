#include "sai_vs.h"

VS_GENERIC_QUAD(BUFFER_POOL,buffer_pool);
VS_GENERIC_QUAD(INGRESS_PRIORITY_GROUP,ingress_priority_group);
VS_GENERIC_QUAD(BUFFER_PROFILE,buffer_profile);
VS_GENERIC_STATS(BUFFER_POOL,buffer_pool);
VS_GENERIC_STATS(INGRESS_PRIORITY_GROUP,ingress_priority_group);

const sai_buffer_api_t vs_buffer_api = {

    VS_GENERIC_QUAD_API(buffer_pool)
    VS_GENERIC_STATS_API(buffer_pool)
    VS_GENERIC_QUAD_API(ingress_priority_group)
    VS_GENERIC_STATS_API(ingress_priority_group)
    VS_GENERIC_QUAD_API(buffer_profile)
};
