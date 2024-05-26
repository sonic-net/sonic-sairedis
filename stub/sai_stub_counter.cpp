#include "sai_stub.h"

STUB_GENERIC_QUAD(COUNTER,counter);
STUB_GENERIC_STATS(COUNTER,counter);

const sai_counter_api_t stub_counter_api = {

    STUB_GENERIC_QUAD_API(counter)
    STUB_GENERIC_STATS_API(counter)
};
