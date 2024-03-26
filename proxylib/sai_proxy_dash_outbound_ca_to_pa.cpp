#include "sai_proxy.h"

PROXY_GENERIC_QUAD_ENTRY(OUTBOUND_CA_TO_PA_ENTRY, outbound_ca_to_pa_entry);
PROXY_BULK_CREATE_ENTRY_EX(OUTBOUND_CA_TO_PA_ENTRY, outbound_ca_to_pa_entry, outbound_ca_to_pa_entries);
PROXY_BULK_REMOVE_ENTRY_EX(OUTBOUND_CA_TO_PA_ENTRY, outbound_ca_to_pa_entry, outbound_ca_to_pa_entries);

const sai_dash_outbound_ca_to_pa_api_t proxy_dash_outbound_ca_to_pa_api = {
    PROXY_GENERIC_QUAD_API(outbound_ca_to_pa_entry)
    proxy_bulk_create_outbound_ca_to_pa_entries,
    proxy_bulk_remove_outbound_ca_to_pa_entries,
};
