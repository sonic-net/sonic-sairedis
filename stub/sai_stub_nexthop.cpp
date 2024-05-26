#include "sai_stub.h"

STUB_GENERIC_QUAD(NEXT_HOP,next_hop);
STUB_BULK_QUAD(NEXT_HOP,next_hop);

const sai_next_hop_api_t stub_next_hop_api = {

    STUB_GENERIC_QUAD_API(next_hop)
    STUB_BULK_QUAD_API(next_hop)
};
