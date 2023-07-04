#include "sai_vs.h"

VS_GENERIC_QUAD_ENTRY(PA_VALIDATION_ENTRY, pa_validation_entry);
VS_BULK_CREATE_ENTRY_EX(PA_VALIDATION_ENTRY, pa_validation_entry, pa_validation_entries);
VS_BULK_REMOVE_ENTRY_EX(PA_VALIDATION_ENTRY, pa_validation_entry, pa_validation_entries);

const sai_dash_pa_validation_api_t vs_dash_pa_validation_api = {
    VS_GENERIC_QUAD_API(pa_validation_entry)
    vs_bulk_create_pa_validation_entries,
    vs_bulk_remove_pa_validation_entries,
};
