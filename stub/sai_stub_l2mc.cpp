#include "sai_stub.h"

STUB_GENERIC_QUAD_ENTRY(L2MC_ENTRY,l2mc_entry);

const sai_l2mc_api_t stub_l2mc_api = {

    STUB_GENERIC_QUAD_API(l2mc_entry)
};
