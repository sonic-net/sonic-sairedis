#include "sai_vs.h"

VS_GENERIC_QUAD_ENTRY(INSEG_ENTRY,inseg_entry);
VS_BULK_QUAD_ENTRY(INSEG_ENTRY,inseg_entry);

const sai_mpls_api_t vs_mpls_api = {

    VS_GENERIC_QUAD_API(inseg_entry)
    VS_BULK_QUAD_API(inseg_entry)
};
