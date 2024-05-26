#include "sai_stub.h"

STUB_BULK_QUAD_ENTRY(NAT_ENTRY,nat_entry);
STUB_GENERIC_QUAD_ENTRY(NAT_ENTRY,nat_entry);
STUB_GENERIC_QUAD(NAT_ZONE_COUNTER,nat_zone_counter);

const sai_nat_api_t stub_nat_api = {

   STUB_GENERIC_QUAD_API(nat_entry)
   STUB_BULK_QUAD_API(nat_entry)
   STUB_GENERIC_QUAD_API(nat_zone_counter)
};
