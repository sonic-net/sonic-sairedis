#include "sai_vs.h"
#include "sai_vs_internal.h"

VS_GENERIC_QUAD(WRED,wred);

const sai_wred_api_t vs_wred_api = {

    VS_GENERIC_QUAD_API(wred)
};
