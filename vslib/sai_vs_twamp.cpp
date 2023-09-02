#include "sai_vs.h"

VS_GENERIC_QUAD(TWAMP_SESSION,twamp_session);
VS_GENERIC_STATS(TWAMP_SESSION,twamp_session);

const sai_twamp_api_t vs_twamp_api = {
    VS_GENERIC_QUAD_API(twamp_session)
    VS_GENERIC_STATS_API(twamp_session)
};
