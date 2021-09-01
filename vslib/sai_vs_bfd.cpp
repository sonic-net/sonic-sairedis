#include "sai_vs.h"

VS_GENERIC_QUAD(BFD_SESSION,bfd_session);
VS_GENERIC_STATS(BFD_SESSION,bfd_session);

const sai_bfd_api_t vs_bfd_api = {

    VS_GENERIC_QUAD_API(bfd_session)
    VS_GENERIC_STATS_API(bfd_session)
};
