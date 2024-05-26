#include "sai_stub.h"

STUB_GENERIC_QUAD_ENTRY(IPMC_ENTRY,ipmc_entry);

const sai_ipmc_api_t stub_ipmc_api = {

    STUB_GENERIC_QUAD_API(ipmc_entry)
};
