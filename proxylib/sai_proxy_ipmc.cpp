#include "sai_proxy.h"

PROXY_GENERIC_QUAD_ENTRY(IPMC_ENTRY,ipmc_entry);

const sai_ipmc_api_t proxy_ipmc_api = {

    PROXY_GENERIC_QUAD_API(ipmc_entry)
};
