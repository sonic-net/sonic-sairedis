#pragma once

#include "SwitchStateBase.h"

#include <atomic>
#include <memory>

namespace saivs
{
    class SwitchMicas48CON2SFP:
        public SwitchStateBase
    {
        public:

            SwitchMicas48CON2SFP(
                    _In_ sai_object_id_t switch_id,
                    _In_ std::shared_ptr<RealObjectIdManager> manager,
                    _In_ std::shared_ptr<SwitchConfig> config);

            SwitchMicas48CON2SFP(
                    _In_ sai_object_id_t switch_id,
                    _In_ std::shared_ptr<RealObjectIdManager> manager,
                    _In_ std::shared_ptr<SwitchConfig> config,
                    _In_ std::shared_ptr<WarmBootState> warmBootState);

            virtual ~SwitchMicas48CON2SFP();

        protected:

            virtual sai_status_t create_cpu_qos_queues(
                    _In_ sai_object_id_t port_id) override;

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

            virtual sai_status_t create_ports() override;

            virtual sai_status_t vs_create_hostif_tap_interface(
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list) override;

            virtual sai_status_t vs_remove_hostif_tap_interface(
                    _In_ sai_object_id_t hostif_id) override;

            virtual bool hostif_create_tap_veth_forwarding(
                    _In_ const std::string &tapname,
                    _In_ int tapfd,
                    _In_ sai_object_id_t port_id) override;

            virtual sai_status_t setPort(
                    _In_ sai_object_id_t portId,
                    _In_ const sai_attribute_t* attr) override;

        private:

            bool renameNetIntf(
                    _In_ const std::string &srcName,
                    _In_ const std::string &dstName);

            void restoreBackingInterfaces();

            bool createEthernetInterface(
                    _In_ const std::string &backingName,
                    _In_ const std::string &ethernetName);

            void scheduleDeferredPortUpNotification(
                    _In_ sai_object_id_t port_id);

        protected:

            constexpr static const uint32_t m_unicastQueueNumber = 10;
            constexpr static const uint32_t m_multicastQueueNumber = 10;

        private:

            std::shared_ptr<std::atomic<bool>> m_shutdown;
    };
}
