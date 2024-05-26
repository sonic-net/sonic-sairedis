#include "sai_stub.h"

STUB_GENERIC_QUAD(BUFFER_POOL,buffer_pool);
STUB_GENERIC_QUAD(INGRESS_PRIORITY_GROUP,ingress_priority_group);
STUB_GENERIC_QUAD(BUFFER_PROFILE,buffer_profile);
STUB_GENERIC_STATS(BUFFER_POOL,buffer_pool);
STUB_GENERIC_STATS(INGRESS_PRIORITY_GROUP,ingress_priority_group);

const sai_buffer_api_t stub_buffer_api = {

    STUB_GENERIC_QUAD_API(buffer_pool)
    STUB_GENERIC_STATS_API(buffer_pool)
    STUB_GENERIC_QUAD_API(ingress_priority_group)
    STUB_GENERIC_STATS_API(ingress_priority_group)
    STUB_GENERIC_QUAD_API(buffer_profile)
};
