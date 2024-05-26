#include "sai_stub.h"

STUB_GENERIC_QUAD(POLICER,policer);
STUB_GENERIC_STATS(POLICER,policer);

const sai_policer_api_t stub_policer_api = {

    STUB_GENERIC_QUAD_API(policer)
    STUB_GENERIC_STATS_API(policer)
};
