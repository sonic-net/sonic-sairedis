#include "MockMeta.h"

using namespace saimeta;

MockMeta::MockMeta(
        _In_ std::shared_ptr<SaiInterface> impl):
    Meta(impl)
{
    SWSS_LOG_ENTER();

    // empty
}

sai_status_t MockMeta::call_meta_validate_stats(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t number_of_counters,
        _In_ const sai_stat_id_t *counter_ids,
        _Out_ uint64_t *counters,
        _In_ sai_stats_mode_t mode)
{
    SWSS_LOG_ENTER();

    return meta_validate_stats(
            object_type,
            object_id,
            number_of_counters,
            counter_ids,
            counters,
            mode);
}
