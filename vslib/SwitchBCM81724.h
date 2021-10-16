#pragma once

#include "SwitchStateBase.h"

namespace saivs
{
    class SwitchBCM81724:
        public SwitchStateBase
    {
        public:

            SwitchBCM81724(
                    _In_ sai_object_id_t switch_id,
                    _In_ std::shared_ptr<RealObjectIdManager> manager,
                    _In_ std::shared_ptr<SwitchConfig> config);

            SwitchBCM81724(
                    _In_ sai_object_id_t switch_id,
                    _In_ std::shared_ptr<RealObjectIdManager> manager,
                    _In_ std::shared_ptr<SwitchConfig> config,
                    _In_ std::shared_ptr<WarmBootState> warmBootState);

            virtual ~SwitchBCM81724();

        protected:

            virtual sai_status_t create_port_dependencies(
                    _In_ sai_object_id_t port_id) override;

            virtual sai_status_t create_default_trap_group() override;

        protected : // refresh

            virtual sai_status_t refresh_port_list(
                    _In_ const sai_attr_metadata_t *meta) override;

        protected:

            virtual sai_status_t refresh_read_only(
                    _In_ const sai_attr_metadata_t *meta,
                    _In_ sai_object_id_t object_id) override;

            virtual sai_status_t set_switch_default_attributes();

        public:

            virtual sai_status_t initialize_default_objects(
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list) override;

            virtual sai_status_t warm_boot_initialize_objects() override;
    };
}
