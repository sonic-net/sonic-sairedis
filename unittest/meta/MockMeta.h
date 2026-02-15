#include "Meta.h"

namespace saimeta
{
    class MockMeta:
        public Meta
    {
        public:

            using sairedis::SaiInterface::set; // name hiding

            MockMeta(
                    _In_ std::shared_ptr<SaiInterface> impl);

            virtual ~MockMeta() = default;

        public:

            sai_status_t call_meta_validate_stats(
                    _In_ sai_object_type_t object_type,
                    _In_ sai_object_id_t object_id,
                    _In_ uint32_t number_of_counters,
                    _In_ const sai_stat_id_t *counter_ids,
                    _Out_ uint64_t *counters,
                    _In_ sai_stats_mode_t mode);
    };
}
