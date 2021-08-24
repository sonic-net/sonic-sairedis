#include "sai_vs.h"

VS_GENERIC_QUAD(DEBUG_COUNTER,debug_counter);

const sai_debug_counter_api_t vs_debug_counter_api = {
    VS_GENERIC_QUAD_API(debug_counter)
};
