#include "sai_redis.h"

REDIS_GENERIC_QUAD_ENTRY(PA_VALIDATION_ENTRY, pa_validation_entry);
REDIS_BULK_CREATE_ENTRY_EX(PA_VALIDATION_ENTRY, pa_validation_entry, pa_validation_entries);
REDIS_BULK_REMOVE_ENTRY_EX(PA_VALIDATION_ENTRY, pa_validation_entry, pa_validation_entries);

const sai_dash_pa_validation_api_t redis_dash_pa_validation_api = {
    REDIS_GENERIC_QUAD_API(pa_validation_entry)
    redis_bulk_create_pa_validation_entries,
    redis_bulk_remove_pa_validation_entries,
};
