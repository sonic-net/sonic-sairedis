#include "sai_redis.h"

REDIS_GENERIC_QUAD(HASH,hash);

const sai_hash_api_t redis_hash_api = {

    REDIS_GENERIC_QUAD_API(hash)
};
