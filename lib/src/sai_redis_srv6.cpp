#include "sai_redis.h"

REDIS_BULK_CREATE(SRV6_SIDLIST, srv6_sidlist);
REDIS_BULK_REMOVE(SRV6_SIDLIST, srv6_sidlist);
REDIS_GENERIC_QUAD(SRV6_SIDLIST,srv6_sidlist);
REDIS_BULK_QUAD_ENTRY(MY_SID_ENTRY,my_sid_entry);
REDIS_GENERIC_QUAD_ENTRY(MY_SID_ENTRY,my_sid_entry);

const sai_srv6_api_t redis_srv6_api = {

    REDIS_GENERIC_QUAD_API(srv6_sidlist)

    redis_bulk_create_srv6_sidlist,
    redis_bulk_remove_srv6_sidlist,

    REDIS_GENERIC_QUAD_API(my_sid_entry)
    REDIS_BULK_QUAD_API(my_sid_entry)
};
