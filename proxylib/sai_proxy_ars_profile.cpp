#include "sai_proxy.h"


PROXY_GENERIC_QUAD(ARS_PROFILE, ars_profile);

const sai_ars_profile_api_t proxy_ars_profile_api = {
    PROXY_GENERIC_QUAD_API(ars_profile)
};
