#include "sai_stub.h"

STUB_GENERIC_QUAD(TWAMP_SESSION,twamp_session);
STUB_GENERIC_STATS(TWAMP_SESSION,twamp_session);

const sai_twamp_api_t stub_twamp_api = {
    STUB_GENERIC_QUAD_API(twamp_session)
    STUB_GENERIC_STATS_API(twamp_session)
};
