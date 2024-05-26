#include "sai_stub.h"

STUB_BULK_CREATE(SRV6_SIDLIST, srv6_sidlist);
STUB_BULK_REMOVE(SRV6_SIDLIST, srv6_sidlist);
STUB_GENERIC_QUAD(SRV6_SIDLIST,srv6_sidlist);
STUB_BULK_QUAD_ENTRY(MY_SID_ENTRY,my_sid_entry);
STUB_GENERIC_QUAD_ENTRY(MY_SID_ENTRY,my_sid_entry);

const sai_srv6_api_t stub_srv6_api = {

    STUB_GENERIC_QUAD_API(srv6_sidlist)

    stub_bulk_create_srv6_sidlist,
    stub_bulk_remove_srv6_sidlist,

    NULL,
    NULL,
    NULL,

    STUB_GENERIC_QUAD_API(my_sid_entry)
    STUB_BULK_QUAD_API(my_sid_entry)
};
