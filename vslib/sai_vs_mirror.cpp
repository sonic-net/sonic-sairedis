#include "sai_vs.h"

VS_GENERIC_QUAD(MIRROR_SESSION,mirror_session);

const sai_mirror_api_t vs_mirror_api = {

    VS_GENERIC_QUAD_API(mirror_session)
};
