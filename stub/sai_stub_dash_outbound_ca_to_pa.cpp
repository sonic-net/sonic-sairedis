#include "sai_stub.h"

STUB_GENERIC_QUAD_ENTRY(OUTBOUND_CA_TO_PA_ENTRY, outbound_ca_to_pa_entry);
STUB_BULK_CREATE_ENTRY_EX(OUTBOUND_CA_TO_PA_ENTRY, outbound_ca_to_pa_entry, outbound_ca_to_pa_entries);
STUB_BULK_REMOVE_ENTRY_EX(OUTBOUND_CA_TO_PA_ENTRY, outbound_ca_to_pa_entry, outbound_ca_to_pa_entries);

const sai_dash_outbound_ca_to_pa_api_t stub_dash_outbound_ca_to_pa_api = {
    STUB_GENERIC_QUAD_API(outbound_ca_to_pa_entry)
    stub_bulk_create_outbound_ca_to_pa_entries,
    stub_bulk_remove_outbound_ca_to_pa_entries,
};
