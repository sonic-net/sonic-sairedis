#pragma once

#include "SwitchStateBase.h"

namespace saivs
{
    class SwitchPoE:
        public SwitchStateBase
    {
        public:

            SwitchPoE(
                    _In_ sai_object_id_t switch_id,
                    _In_ std::shared_ptr<RealObjectIdManager> manager,
                    _In_ std::shared_ptr<SwitchConfig> config);

            SwitchPoE(
                    _In_ sai_object_id_t switch_id,
                    _In_ std::shared_ptr<RealObjectIdManager> manager,
                    _In_ std::shared_ptr<SwitchConfig> config,
                    _In_ std::shared_ptr<WarmBootState> warmBootState);

            virtual ~SwitchPoE();

            virtual sai_status_t create(
                    _In_ sai_object_type_t object_type,
                    _In_ const std::string &serializedObjectId,
                    _In_ sai_object_id_t switch_id,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list) override;

        protected:
            std::vector<sai_object_id_t> m_poe_device_list;
            std::vector<sai_object_id_t> m_poe_pse_list;
            std::vector<sai_object_id_t> m_poe_port_list;

            sai_status_t createPoeDevice(
                    _In_ sai_object_id_t object_id,
                    _In_ sai_object_id_t switch_id,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

            sai_status_t createPoePort(
                    _In_ sai_object_id_t object_id,
                    _In_ sai_object_id_t switch_id,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

            virtual sai_status_t refresh_read_only(
                    _In_ const sai_attr_metadata_t *meta,
                    _In_ sai_object_id_t object_id) override;

            virtual sai_status_t refresh_poe_pse_list(
                    _In_ const sai_attr_metadata_t *meta,
                    _In_ sai_object_id_t poe_device_id);

            virtual sai_status_t refresh_poe_port_list(
                    _In_ const sai_attr_metadata_t *meta,
                    _In_ sai_object_id_t poe_device_id);

        public:
            virtual sai_status_t set_switch_default_attributes();
            virtual sai_status_t initialize_default_objects(
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list) override;

    };
}
