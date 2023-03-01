#include "sai_vs.h"

VS_GENERIC_QUAD_ENTRY(OUTBOUND_CA_TO_PA_ENTRY, outbound_ca_to_pa_entry);
VS_BULK_CREATE_ENTRY_EX(OUTBOUND_CA_TO_PA_ENTRY, outbound_ca_to_pa_entry, outbound_ca_to_pa_entries);
VS_BULK_REMOVE_ENTRY_EX(OUTBOUND_CA_TO_PA_ENTRY, outbound_ca_to_pa_entry, outbound_ca_to_pa_entries);

const sai_dash_outbound_ca_to_pa_api_t vs_dash_outbound_ca_to_pa_api = {
    VS_GENERIC_QUAD_API(outbound_ca_to_pa_entry)
    vs_bulk_create_outbound_ca_to_pa_entries,
    vs_bulk_remove_outbound_ca_to_pa_entries,
};
