#include "sai_stub.h"

STUB_GENERIC_QUAD(HASH,hash);
STUB_GENERIC_QUAD(FINE_GRAINED_HASH_FIELD,fine_grained_hash_field);

const sai_hash_api_t stub_hash_api = {
    STUB_GENERIC_QUAD_API(hash)
    STUB_GENERIC_QUAD_API(fine_grained_hash_field)
};
