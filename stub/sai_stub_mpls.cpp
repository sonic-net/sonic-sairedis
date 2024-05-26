#include "sai_stub.h"

STUB_GENERIC_QUAD_ENTRY(INSEG_ENTRY,inseg_entry);
STUB_BULK_QUAD_ENTRY(INSEG_ENTRY,inseg_entry);

const sai_mpls_api_t stub_mpls_api = {

    STUB_GENERIC_QUAD_API(inseg_entry)
    STUB_BULK_QUAD_API(inseg_entry)
};
