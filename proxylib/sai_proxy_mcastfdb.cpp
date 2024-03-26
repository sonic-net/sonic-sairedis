#include "sai_proxy.h"

PROXY_GENERIC_QUAD_ENTRY(MCAST_FDB_ENTRY,mcast_fdb_entry);

const sai_mcast_fdb_api_t proxy_mcast_fdb_api = {

    PROXY_GENERIC_QUAD_API(mcast_fdb_entry)
};
