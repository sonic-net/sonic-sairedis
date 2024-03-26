#include "sai_proxy.h"

PROXY_GENERIC_QUAD(MIRROR_SESSION,mirror_session);

const sai_mirror_api_t proxy_mirror_api = {

    PROXY_GENERIC_QUAD_API(mirror_session)
};
