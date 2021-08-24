#include "sai_vs.h"

VS_BULK_CREATE(SEGMENTROUTE_SIDLIST,segmentroute_sidlists);
VS_BULK_REMOVE(SEGMENTROUTE_SIDLIST,segmentroute_sidlists);

VS_GENERIC_QUAD(SEGMENTROUTE_SIDLIST,segmentroute_sidlist);

const sai_segmentroute_api_t vs_segmentroute_api = {

    VS_GENERIC_QUAD_API(segmentroute_sidlist)

    vs_bulk_create_segmentroute_sidlists,
    vs_bulk_remove_segmentroute_sidlists,
};
