#include "sai_vs.h"

VS_GENERIC_QUAD_ENTRY(L2MC_ENTRY,l2mc_entry);

const sai_l2mc_api_t vs_l2mc_api = {

    VS_GENERIC_QUAD_API(l2mc_entry)
};
