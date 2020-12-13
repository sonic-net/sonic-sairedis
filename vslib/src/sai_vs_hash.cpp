#include "sai_vs.h"

VS_GENERIC_QUAD(HASH,hash);
VS_GENERIC_QUAD(FINE_GRAINED_HASH_FIELD,fine_grained_hash_field);

const sai_hash_api_t vs_hash_api = {
    VS_GENERIC_QUAD_API(hash)
    VS_GENERIC_QUAD_API(fine_grained_hash_field)
};
