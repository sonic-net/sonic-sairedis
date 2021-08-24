#include "sai_vs.h"

VS_GENERIC_QUAD_ENTRY(IPMC_ENTRY,ipmc_entry);

const sai_ipmc_api_t vs_ipmc_api = {

    VS_GENERIC_QUAD_API(ipmc_entry)
};
