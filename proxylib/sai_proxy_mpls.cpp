#include "sai_proxy.h"

PROXY_GENERIC_QUAD_ENTRY(INSEG_ENTRY,inseg_entry);
PROXY_BULK_QUAD_ENTRY(INSEG_ENTRY,inseg_entry);

const sai_mpls_api_t proxy_mpls_api = {

    PROXY_GENERIC_QUAD_API(inseg_entry)
    PROXY_BULK_QUAD_API(inseg_entry)
};
