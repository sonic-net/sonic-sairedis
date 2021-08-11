#include "sai_redis.h"

REDIS_BULK_QUAD_ENTRY(NAT_ENTRY,nat_entry);
REDIS_GENERIC_QUAD_ENTRY(NAT_ENTRY,nat_entry);
REDIS_GENERIC_QUAD(NAT_ZONE_COUNTER,nat_zone_counter);

const sai_nat_api_t redis_nat_api = {

   REDIS_GENERIC_QUAD_API(nat_entry)
   REDIS_BULK_QUAD_API(nat_entry)
   REDIS_GENERIC_QUAD_API(nat_zone_counter)
};
