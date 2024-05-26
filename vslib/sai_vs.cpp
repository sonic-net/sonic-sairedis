#include "sai_stub.h"
#include "Sai.h"

std::shared_ptr<sairedis::SaiInterface> stub_sai = std::make_shared<saivs::Sai>();

namespace saivs
{
    sai_global_apis_t ga =
    {
        .api_initialize = &sai_api_initialize,
        .api_query = &sai_api_query,
        .api_uninitialize = &sai_api_uninitialize,
        .bulk_get_attribute = nullptr,
        .bulk_object_clear_stats = &sai_bulk_object_clear_stats,
        .bulk_object_get_stats = &sai_bulk_object_get_stats,
        .dbg_generate_dump = nullptr,
        .get_maximum_attribute_count = nullptr,
        .get_object_count = nullptr,
        .get_object_key = nullptr,
        .log_set = &sai_log_set,
        .object_type_get_availability = &sai_object_type_get_availability,
        .object_type_query = &sai_object_type_query,
        .query_api_version = &sai_query_api_version,
        .query_attribute_capability = &sai_query_attribute_capability,
        .query_attribute_enum_values_capability = &sai_query_attribute_enum_values_capability,
        .query_object_stage = nullptr,
        .query_stats_capability = &sai_query_stats_capability,
        .switch_id_query = &sai_switch_id_query,
        .tam_telemetry_get_data = nullptr,
    };
}
