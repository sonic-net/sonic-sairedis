#include "sai_vs.h"

VS_GENERIC_QUAD(NEXT_HOP,next_hop);

const sai_next_hop_api_t vs_next_hop_api = {

    VS_GENERIC_QUAD_API(next_hop)
};
