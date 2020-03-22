#include "sai_vs.h"
#include "sai_vs_internal.h"

VS_GENERIC_QUAD_ENTRY(MCAST_FDB_ENTRY,mcast_fdb_entry);

const sai_mcast_fdb_api_t vs_mcast_fdb_api = {

    VS_GENERIC_QUAD_API(mcast_fdb_entry)
};
