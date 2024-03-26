#include "sai_proxy.h"

PROXY_GENERIC_QUAD_ENTRY(PA_VALIDATION_ENTRY, pa_validation_entry);
PROXY_BULK_CREATE_ENTRY_EX(PA_VALIDATION_ENTRY, pa_validation_entry, pa_validation_entries);
PROXY_BULK_REMOVE_ENTRY_EX(PA_VALIDATION_ENTRY, pa_validation_entry, pa_validation_entries);

const sai_dash_pa_validation_api_t proxy_dash_pa_validation_api = {
    PROXY_GENERIC_QUAD_API(pa_validation_entry)
    proxy_bulk_create_pa_validation_entries,
    proxy_bulk_remove_pa_validation_entries,
};
