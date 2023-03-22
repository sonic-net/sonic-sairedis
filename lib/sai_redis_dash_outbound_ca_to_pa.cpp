#include "sai_redis.h"

REDIS_GENERIC_QUAD_ENTRY(OUTBOUND_CA_TO_PA_ENTRY, outbound_ca_to_pa_entry);
REDIS_BULK_CREATE_ENTRY_EX(OUTBOUND_CA_TO_PA_ENTRY, outbound_ca_to_pa_entry, outbound_ca_to_pa_entries);
REDIS_BULK_REMOVE_ENTRY_EX(OUTBOUND_CA_TO_PA_ENTRY, outbound_ca_to_pa_entry, outbound_ca_to_pa_entries);

const sai_dash_outbound_ca_to_pa_api_t redis_dash_outbound_ca_to_pa_api = {
    REDIS_GENERIC_QUAD_API(outbound_ca_to_pa_entry)
    redis_bulk_create_outbound_ca_to_pa_entries,
    redis_bulk_remove_outbound_ca_to_pa_entries,
};
