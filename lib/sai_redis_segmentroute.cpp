#include "sai_redis.h"

REDIS_BULK_CREATE(SEGMENTROUTE_SIDLIST,segmentroute_sidlists);
REDIS_BULK_REMOVE(SEGMENTROUTE_SIDLIST,segmentroute_sidlists);
REDIS_GENERIC_QUAD(SEGMENTROUTE_SIDLIST,segmentroute_sidlist);

const sai_segmentroute_api_t redis_segmentroute_api = {

    REDIS_GENERIC_QUAD_API(segmentroute_sidlist)

    redis_bulk_create_segmentroute_sidlists,
    redis_bulk_remove_segmentroute_sidlists,
};
