#include "sai_proxy.h"

PROXY_BULK_CREATE(SRV6_SIDLIST, srv6_sidlist);
PROXY_BULK_REMOVE(SRV6_SIDLIST, srv6_sidlist);
PROXY_GENERIC_QUAD(SRV6_SIDLIST,srv6_sidlist);
PROXY_BULK_QUAD_ENTRY(MY_SID_ENTRY,my_sid_entry);
PROXY_GENERIC_QUAD_ENTRY(MY_SID_ENTRY,my_sid_entry);

const sai_srv6_api_t proxy_srv6_api = {

    PROXY_GENERIC_QUAD_API(srv6_sidlist)

    proxy_bulk_create_srv6_sidlist,
    proxy_bulk_remove_srv6_sidlist,

    NULL,
    NULL,
    NULL,

    PROXY_GENERIC_QUAD_API(my_sid_entry)
    PROXY_BULK_QUAD_API(my_sid_entry)
};
