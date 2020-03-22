#include "sai_vs.h"
#include "sai_vs_internal.h"

VS_GENERIC_QUAD_ENTRY(INSEG_ENTRY,inseg_entry);

const sai_mpls_api_t vs_mpls_api = {

    VS_GENERIC_QUAD_API(inseg_entry)
};
