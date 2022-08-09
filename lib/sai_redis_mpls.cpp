#include "sai_redis.h"

REDIS_GENERIC_QUAD_ENTRY(INSEG_ENTRY,inseg_entry);
REDIS_BULK_QUAD_ENTRY(INSEG_ENTRY,inseg_entry);

const sai_mpls_api_t redis_mpls_api = {

    REDIS_GENERIC_QUAD_API(inseg_entry)
    REDIS_BULK_QUAD_API(inseg_entry)
};
