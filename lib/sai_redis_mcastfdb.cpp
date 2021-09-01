#include "sai_redis.h"

REDIS_GENERIC_QUAD_ENTRY(MCAST_FDB_ENTRY,mcast_fdb_entry);

const sai_mcast_fdb_api_t redis_mcast_fdb_api = {

    REDIS_GENERIC_QUAD_API(mcast_fdb_entry)
};
