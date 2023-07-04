#pragma once

#include "SwitchStateBase.h"

namespace saivs
{
    class SwitchNvdaMBF2H536C:
        public SwitchStateBase
    {
        public:

            SwitchNvdaMBF2H536C(
                    _In_ sai_object_id_t switch_id,
                    _In_ std::shared_ptr<RealObjectIdManager> manager,
                    _In_ std::shared_ptr<SwitchConfig> config);

            SwitchNvdaMBF2H536C(
                    _In_ sai_object_id_t switch_id,
                    _In_ std::shared_ptr<RealObjectIdManager> manager,
                    _In_ std::shared_ptr<SwitchConfig> config,
                    _In_ std::shared_ptr<WarmBootState> warmBootState);

            virtual ~SwitchNvdaMBF2H536C() = default;

        public:

                void processFdbEntriesForAging();

                virtual sai_status_t initialize_default_objects(
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

    };
}
