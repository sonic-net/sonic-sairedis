#include "sai_vs.h"

VS_BULK_CREATE(SRV6_SIDLIST,srv6_sidlists);
VS_BULK_REMOVE(SRV6_SIDLIST,srv6_sidlists);

VS_GENERIC_QUAD(SRV6_SIDLIST,srv6_sidlist);

VS_GENERIC_QUAD_ENTRY(MY_SID_ENTRY, my_sid_entry);
VS_BULK_QUAD_ENTRY(MY_SID_ENTRY, my_sid_entry);

const sai_srv6_api_t vs_srv6_api = {

    VS_GENERIC_QUAD_API(srv6_sidlist)

    vs_bulk_create_srv6_sidlists,
    vs_bulk_remove_srv6_sidlists,

    VS_GENERIC_QUAD_API(my_sid_entry)
    VS_BULK_QUAD_API(my_sid_entry)
};
