#include "sai_proxy.h"

PROXY_GENERIC_QUAD_ENTRY(L2MC_ENTRY,l2mc_entry);

const sai_l2mc_api_t proxy_l2mc_api = {

    PROXY_GENERIC_QUAD_API(l2mc_entry)
};
