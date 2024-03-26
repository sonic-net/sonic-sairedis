#include "sai_proxy.h"

PROXY_BULK_QUAD_ENTRY(NAT_ENTRY,nat_entry);
PROXY_GENERIC_QUAD_ENTRY(NAT_ENTRY,nat_entry);
PROXY_GENERIC_QUAD(NAT_ZONE_COUNTER,nat_zone_counter);

const sai_nat_api_t proxy_nat_api = {

   PROXY_GENERIC_QUAD_API(nat_entry)
   PROXY_BULK_QUAD_API(nat_entry)
   PROXY_GENERIC_QUAD_API(nat_zone_counter)
};
