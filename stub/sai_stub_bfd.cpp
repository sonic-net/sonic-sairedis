#include "sai_stub.h"

STUB_GENERIC_QUAD(BFD_SESSION,bfd_session);
STUB_GENERIC_STATS(BFD_SESSION,bfd_session);

const sai_bfd_api_t stub_bfd_api = {

    STUB_GENERIC_QUAD_API(bfd_session)
    STUB_GENERIC_STATS_API(bfd_session)
};
