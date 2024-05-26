#include "sai_stub.h"

STUB_GENERIC_QUAD_ENTRY(PA_VALIDATION_ENTRY, pa_validation_entry);
STUB_BULK_CREATE_ENTRY_EX(PA_VALIDATION_ENTRY, pa_validation_entry, pa_validation_entries);
STUB_BULK_REMOVE_ENTRY_EX(PA_VALIDATION_ENTRY, pa_validation_entry, pa_validation_entries);

const sai_dash_pa_validation_api_t stub_dash_pa_validation_api = {
    STUB_GENERIC_QUAD_API(pa_validation_entry)
    stub_bulk_create_pa_validation_entries,
    stub_bulk_remove_pa_validation_entries,
};
