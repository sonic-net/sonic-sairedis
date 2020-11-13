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

            virtual sai_status_t create_port_dependencies(_In_ sai_object_id_t port_id) override;

            virtual sai_status_t create_qos_queues_per_port(_In_ sai_object_id_t port_id) override;

            virtual sai_status_t create_qos_queues() override;

            virtual sai_status_t set_switch_mac_address() override;

            virtual sai_status_t create_default_vlan() override;

            virtual sai_status_t create_default_1q_bridge() override;

            virtual sai_status_t create_default_virtual_router() override;

            virtual sai_status_t create_default_stp_instance() override;

            virtual sai_status_t create_default_trap_group() override;

            virtual sai_status_t create_ingress_priority_groups_per_port(
                    _In_ sai_object_id_t port_id) override;

            virtual sai_status_t create_ingress_priority_groups() override;

            virtual sai_status_t create_vlan_members() override;

            virtual sai_status_t create_bridge_ports() override;

            virtual sai_status_t set_acl_entry_min_prio() override;

            virtual sai_status_t set_acl_capabilities() override;

            virtual sai_status_t set_maximum_number_of_childs_per_scheduler_group() override;

            virtual sai_status_t set_number_of_ecmp_groups() override; 

            virtual sai_status_t create_cpu_port();

            virtual sai_status_t create_ports();

       protected : // refresh

            virtual sai_status_t refresh_port_list(
                    _In_ const sai_attr_metadata_t *meta) override;

            virtual sai_status_t refresh_bridge_port_list(
                    _In_ const sai_attr_metadata_t *meta,
                    _In_ sai_object_id_t bridge_id) override;

            virtual sai_status_t refresh_vlan_member_list(
                    _In_ const sai_attr_metadata_t *meta,
                    _In_ sai_object_id_t vlan_id) override;

        protected:

            virtual sai_status_t refresh_read_only(
                    _In_ const sai_attr_metadata_t *meta,
                    _In_ sai_object_id_t object_id) override;

            virtual sai_status_t set_switch_default_attributes();

            virtual sai_status_t initialize_default_objects(
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list) override;
    };
}
