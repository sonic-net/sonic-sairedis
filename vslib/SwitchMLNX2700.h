#pragma once

#include "SwitchStateBase.h"

namespace saivs
{
    class SwitchMLNX2700:
        public SwitchStateBase
    {
        public:

            SwitchMLNX2700(
                    _In_ sai_object_id_t switch_id,
                    _In_ std::shared_ptr<RealObjectIdManager> manager,
                    _In_ std::shared_ptr<SwitchConfig> config);

            SwitchMLNX2700(
                    _In_ sai_object_id_t switch_id,
                    _In_ std::shared_ptr<RealObjectIdManager> manager,
                    _In_ std::shared_ptr<SwitchConfig> config,
                    _In_ std::shared_ptr<WarmBootState> warmBootState);

            virtual ~SwitchMLNX2700() = default;

        protected:

            virtual sai_status_t create_qos_queues_per_port(
                    _In_ sai_object_id_t port_id) override;

            virtual sai_status_t create_qos_queues() override;

            virtual sai_status_t set_number_of_queues() override;

            virtual sai_status_t create_scheduler_group_tree(
                    _In_ const std::vector<sai_object_id_t>& sgs,
                    _In_ sai_object_id_t port_id) override;

            virtual sai_status_t create_scheduler_groups_per_port(
                    _In_ sai_object_id_t port_id) override;

            virtual sai_status_t set_maximum_number_of_childs_per_scheduler_group() override;

            virtual sai_status_t refresh_bridge_port_list(
                    _In_ const sai_attr_metadata_t *meta,
                    _In_ sai_object_id_t bridge_id) override;

            virtual sai_status_t warm_update_queues() override;

            virtual sai_status_t create_port_serdes() override;

            virtual sai_status_t create_port_serdes_per_port(
                    _In_ sai_object_id_t port_id) override;

        protected:
            virtual sai_status_t queryTunnelPeerModeCapability(
                                      _Inout_ sai_s32_list_t *enum_values_capability) override;
            virtual sai_status_t queryVlanfloodTypeCapability(
                                      _Inout_ sai_s32_list_t *enum_values_capability) override;
            virtual sai_status_t queryPortAutonegFecOverrideSupportCapability(
                                      _Out_ sai_attr_capability_t *attr_capability) override;

        protected:
            constexpr static const uint32_t m_unicastQueueNumber = 8;
            constexpr static const uint32_t m_multicastQueueNumber = 8;
    };
}
