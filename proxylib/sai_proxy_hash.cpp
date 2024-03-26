#include "sai_proxy.h"

PROXY_GENERIC_QUAD(HASH,hash);
PROXY_GENERIC_QUAD(FINE_GRAINED_HASH_FIELD,fine_grained_hash_field);

const sai_hash_api_t proxy_hash_api = {
    PROXY_GENERIC_QUAD_API(hash)
    PROXY_GENERIC_QUAD_API(fine_grained_hash_field)
};
