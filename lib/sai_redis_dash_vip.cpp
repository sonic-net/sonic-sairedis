#include "sai_redis.h"

REDIS_GENERIC_QUAD_ENTRY(VIP_ENTRY, vip_entry);
REDIS_BULK_CREATE_ENTRY_EX(VIP_ENTRY, vip_entry, vip_entries);
REDIS_BULK_REMOVE_ENTRY_EX(VIP_ENTRY, vip_entry, vip_entries);

const sai_dash_vip_api_t redis_dash_vip_api = {
    REDIS_GENERIC_QUAD_API(vip_entry)
    redis_bulk_create_vip_entries,
    redis_bulk_remove_vip_entries,
};
