#include "sai_vs.h"

VS_BULK_QUAD_ENTRY(NAT_ENTRY,nat_entry);

VS_GENERIC_QUAD_ENTRY(NAT_ENTRY,nat_entry);
VS_GENERIC_QUAD(NAT_ZONE_COUNTER,nat_zone_counter);

const sai_nat_api_t vs_nat_api = {

   VS_GENERIC_QUAD_API(nat_entry)

   VS_BULK_QUAD_API(nat_entry)

   VS_GENERIC_QUAD_API(nat_zone_counter)
};
