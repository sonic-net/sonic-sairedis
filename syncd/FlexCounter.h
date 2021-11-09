#pragma once

extern "C" {
#include "sai.h"
}

#include "meta/SaiInterface.h"

#include "swss/table.h"

#include <vector>
#include <set>
#include <condition_variable>
#include <unordered_map>
#include <memory>

namespace syncd
{
    class FlexCounter
    {
        private:

            FlexCounter(const FlexCounter&) = delete;

        public:

            FlexCounter(
                    _In_ const std::string& instanceId,
                    _In_ std::shared_ptr<sairedis::SaiInterface> vendorSai,
                    _In_ const std::string& dbCounters);

            virtual ~FlexCounter();

        public:

            void addCounterPlugin(
                    _In_ const std::vector<swss::FieldValueTuple>& values);

            void removeCounterPlugins();

            void addCounter(
                    _In_ sai_object_id_t vid,
                    _In_ sai_object_id_t rid,
                    _In_ const std::vector<swss::FieldValueTuple>& values);

            void removeCounter(
                    _In_ sai_object_id_t vid);

            bool isEmpty();

            bool isDiscarded();

        private:

            void setPollInterval(
                    _In_ uint32_t pollInterval);

            void setStatus(
                    _In_ const std::string& status);

            void setStatsMode(
                    _In_ const std::string& mode);

        private: // plugins

            void addPortCounterPlugin(
                    _In_ const std::string& sha);

            void addRifCounterPlugin(
                    _In_ const std::string& sha);

            void addBufferPoolCounterPlugin(
                    _In_ const std::string& sha);

            void addPriorityGroupCounterPlugin(
                    _In_ const std::string& sha);

            void addQueueCounterPlugin(
                    _In_ const std::string& sha);

            void addTunnelCounterPlugin(
                    _In_ const std::string& sha);

            void addFlowCounterPlugin(
                    _In_ const std::string& sha);

        private:

            void checkPluginRegistered(
                    _In_ const std::string& sha) const;

            bool allIdsEmpty() const;

            bool allPluginsEmpty() const;

        private: // remove counter

            void removePort(
                    _In_ sai_object_id_t portVid);

            void removePortDebugCounters(
                    _In_ sai_object_id_t portVid);

            void removeQueue(
                    _In_ sai_object_id_t queueVid);

            void removePriorityGroup(
                    _In_ sai_object_id_t priorityGroupVid);

            void removeRif(
                    _In_ sai_object_id_t rifVid);

            void removeBufferPool(
                    _In_ sai_object_id_t bufferPoolVid);

            void removeSwitchDebugCounters(
                    _In_ sai_object_id_t switchVid);

            void removeMACsecSA(
                    _In_ sai_object_id_t macsecSAVid);

            void removeAclCounter(
                    _In_ sai_object_id_t aclCounterVid);

            void removeTunnel(
                    _In_ sai_object_id_t tunnelVid);

            void removeFlowCounter(
                    _In_ sai_object_id_t counterVid);

        private: // set counter list

            void setPortCounterList(
                    _In_ sai_object_id_t portVid,
                    _In_ sai_object_id_t portRid,
                    _In_ const std::vector<sai_port_stat_t> &counterIds);

            void setPortDebugCounterList(
                    _In_ sai_object_id_t portVid,
                    _In_ sai_object_id_t portRid,
                    _In_ const std::vector<sai_port_stat_t> &counterIds);

            void setQueueCounterList(
                    _In_ sai_object_id_t queueVid,
                    _In_ sai_object_id_t queueRid,
                    _In_ const std::vector<sai_queue_stat_t> &counterIds);

            void setPriorityGroupCounterList(
                    _In_ sai_object_id_t priorityGroupVid,
                    _In_ sai_object_id_t priorityGroupRid,
                    _In_ const std::vector<sai_ingress_priority_group_stat_t> &counterIds);

            void setRifCounterList(
                    _In_ sai_object_id_t rifVid,
                    _In_ sai_object_id_t rifRid,
                    _In_ const std::vector<sai_router_interface_stat_t> &counterIds);

            void setSwitchDebugCounterList(
                    _In_ sai_object_id_t switchVid,
                    _In_ sai_object_id_t switchRid,
                    _In_ const std::vector<sai_switch_stat_t> &counterIds);

            void setBufferPoolCounterList(
                    _In_ sai_object_id_t bufferPoolVid,
                    _In_ sai_object_id_t bufferPoolRid,
                    _In_ const std::vector<sai_buffer_pool_stat_t>& counterIds,
                    _In_ const std::string& statsMode);

            void setTunnelCounterList(
                    _In_ sai_object_id_t tunnelVid,
                    _In_ sai_object_id_t tunnelRid,
                    _In_ const std::vector<sai_tunnel_stat_t> &counterIds);

             void setFlowCounterList(
                    _In_ sai_object_id_t counterVid,
                    _In_ sai_object_id_t counterRid,
                    _In_ const std::vector<sai_counter_stat_t>& counterIds);

        private: // set attr list

            void setQueueAttrList(
                    _In_ sai_object_id_t queueVid,
                    _In_ sai_object_id_t queueRid,
                    _In_ const std::vector<sai_queue_attr_t> &attrIds);

            void setPriorityGroupAttrList(
                    _In_ sai_object_id_t priorityGroupVid,
                    _In_ sai_object_id_t priorityGroupRid,
                    _In_ const std::vector<sai_ingress_priority_group_attr_t> &attrIds);

            void setMACsecSAAttrList(
                    _In_ sai_object_id_t macsecSAVid,
                    _In_ sai_object_id_t macsecSARid,
                    _In_ const std::vector<sai_macsec_sa_attr_t> &attrIds);

            void setAclCounterAttrList(
                    _In_ sai_object_id_t aclCounterVid,
                    _In_ sai_object_id_t aclCounterRid,
                    _In_ const std::vector<sai_acl_counter_attr_t> &attrIds);

        private: // is counter supported

            bool isPortCounterSupported(
                    _In_ sai_port_stat_t counter) const;

            bool isPriorityGroupCounterSupported(
                    _In_ sai_ingress_priority_group_stat_t counter) const;

            bool isQueueCounterSupported(
                    _In_ sai_queue_stat_t counter) const;

            bool isRifCounterSupported(
                    _In_ sai_router_interface_stat_t counter) const;

            bool isBufferPoolCounterSupported(
                    _In_ sai_buffer_pool_stat_t counter) const;

            bool isTunnelCounterSupported(
                    _In_ sai_tunnel_stat_t counter) const;

            bool isStatsModeSupported(
                    _In_ uint32_t statsMode,
                    _In_ sai_stats_mode_t statCapability);

        private: // update supported counters

            sai_status_t querySupportedPortCounters(
                    _In_ sai_object_id_t portRid);

            void getSupportedPortCounters(
                    _In_ sai_object_id_t portRid);

            void updateSupportedPortCounters(
                    _In_ sai_object_id_t portRid);

            std::vector<sai_port_stat_t> saiCheckSupportedPortDebugCounters(
                    _In_ sai_object_id_t portRid,
                    _In_ const std::vector<sai_port_stat_t> &counterIds);

            sai_status_t querySupportedQueueCounters(
                    _In_ sai_object_id_t queueId);

            void getSupportedQueueCounters(
                    _In_ sai_object_id_t queueId, const std::vector<sai_queue_stat_t> &counterIds);

            void updateSupportedQueueCounters(
                    _In_ sai_object_id_t queueRid,
                    _In_ const std::vector<sai_queue_stat_t> &counterIds);

            sai_status_t querySupportedRifCounters(
                    _In_ sai_object_id_t rifRid);

            void getSupportedRifCounters(
                    _In_ sai_object_id_t rifRid);

            void updateSupportedRifCounters(
                    _In_ sai_object_id_t rifRid);

            sai_status_t querySupportedBufferPoolCounters(
                    _In_ sai_object_id_t bufferPoolId,
                    _In_ sai_stats_mode_t statsMode);

            void getSupportedBufferPoolCounters(
                    _In_ sai_object_id_t bufferPoolId,
                    _In_ const std::vector<sai_buffer_pool_stat_t> &counterIds,
                    _In_ sai_stats_mode_t statsMode);

            void updateSupportedBufferPoolCounters(
                    _In_ sai_object_id_t bufferPoolRid,
                    _In_ const std::vector<sai_buffer_pool_stat_t> &counterIds,
                    _In_ sai_stats_mode_t statsMode);

            sai_status_t querySupportedPriorityGroupCounters(
                    _In_ sai_object_id_t priorityGroupRid);

            void getSupportedPriorityGroupCounters(
                    _In_ sai_object_id_t priorityGroupRid,
                    _In_ const std::vector<sai_ingress_priority_group_stat_t> &counterIds);

            void updateSupportedPriorityGroupCounters(
                    _In_ sai_object_id_t priorityGroupRid,
                    _In_ const std::vector<sai_ingress_priority_group_stat_t> &counterIds);

            void updateSupportedFlowCounters(
                    _In_ sai_object_id_t counterRid,
                    _In_ const std::vector<sai_counter_stat_t> &counterIds);

            std::vector<sai_switch_stat_t> saiCheckSupportedSwitchDebugCounters(
                    _In_ sai_object_id_t switchRid,
                    _In_ const std::vector<sai_switch_stat_t> &counterIds);

            void updateSupportedTunnelCounters(
                    _In_ sai_object_id_t tunnelRid,
                    _In_ const std::vector<sai_tunnel_stat_t> &counterIds);
        private:

            struct QueueCounterIds
            {
                QueueCounterIds(
                        _In_ sai_object_id_t queue,
                        _In_ const std::vector<sai_queue_stat_t> &queueIds);

                sai_object_id_t queueId;
                std::vector<sai_queue_stat_t> queueCounterIds;
            };

            struct QueueAttrIds
            {
                QueueAttrIds(
                        _In_ sai_object_id_t queue,
                        _In_ const std::vector<sai_queue_attr_t> &queueIds);

                sai_object_id_t queueId;
                std::vector<sai_queue_attr_t> queueAttrIds;
            };

            struct IngressPriorityGroupCounterIds
            {
                IngressPriorityGroupCounterIds(
                        _In_ sai_object_id_t priorityGroup,
                        _In_ const std::vector<sai_ingress_priority_group_stat_t> &priorityGroupIds);

                sai_object_id_t priorityGroupId;
                std::vector<sai_ingress_priority_group_stat_t> priorityGroupCounterIds;
            };

            struct IngressPriorityGroupAttrIds
            {
                IngressPriorityGroupAttrIds(
                        _In_ sai_object_id_t priorityGroup,
                        _In_ const std::vector<sai_ingress_priority_group_attr_t> &priorityGroupIds);

                sai_object_id_t priorityGroupId;
                std::vector<sai_ingress_priority_group_attr_t> priorityGroupAttrIds;
            };

            struct BufferPoolCounterIds
            {
                BufferPoolCounterIds(
                        _In_ sai_object_id_t bufferPool,
                        _In_ const std::vector<sai_buffer_pool_stat_t> &bufferPoolIds,
                        _In_ sai_stats_mode_t statsMode);

                sai_object_id_t bufferPoolId;
                sai_stats_mode_t bufferPoolStatsMode;
                std::vector<sai_buffer_pool_stat_t> bufferPoolCounterIds;
            };

            struct PortCounterIds
            {
                PortCounterIds(
                        _In_ sai_object_id_t port,
                        _In_ const std::vector<sai_port_stat_t> &portIds);

                sai_object_id_t portId;
                std::vector<sai_port_stat_t> portCounterIds;
            };

            struct SwitchCounterIds
            {
                SwitchCounterIds(
                        _In_ sai_object_id_t oid,
                        _In_ const std::vector<sai_switch_stat_t> &counterIds);

                sai_object_id_t switchId;
                std::vector<sai_switch_stat_t> switchCounterIds;
            };

            struct RifCounterIds
            {
                RifCounterIds(
                        _In_ sai_object_id_t rif,
                        _In_ const std::vector<sai_router_interface_stat_t> &rifIds);

                sai_object_id_t rifId;
                std::vector<sai_router_interface_stat_t> rifCounterIds;
            };

            struct MACsecSAAttrIds
            {
                MACsecSAAttrIds(
                        _In_ sai_object_id_t macsecSA,
                        _In_ const std::vector<sai_macsec_sa_attr_t> &macsecSAIds);

                sai_object_id_t m_macsecSAId;
                std::vector<sai_macsec_sa_attr_t> m_macsecSAAttrIds;
            };

            struct AclCounterAttrIds
            {
                AclCounterAttrIds(
                        _In_ sai_object_id_t aclCounter,
                        _In_ const std::vector<sai_acl_counter_attr_t> &aclCounterIds);

                sai_object_id_t m_aclCounterId;
                std::vector<sai_acl_counter_attr_t> m_aclCounterAttrIds;
            };

            struct TunnelCounterIds
            {
                TunnelCounterIds(
                        _In_ sai_object_id_t tunnel,
                        _In_ const std::vector<sai_tunnel_stat_t> &tunnelIds);

                sai_object_id_t m_tunnelId;
                std::vector<sai_tunnel_stat_t> m_tunnelCounterIds;
            };

            struct FlowCounterIds
            {
                FlowCounterIds(
                        _In_ sai_object_id_t counterId,
                        _In_ const std::vector<sai_counter_stat_t> &flowCounterIds);

                sai_object_id_t counterId;
                std::vector<sai_counter_stat_t> flowCounterIds;
            };

        private:

            void collectCounters(
                    _In_ swss::Table &countersTable);

            void runPlugins(
                    _In_ swss::DBConnector& db);

            void startFlexCounterThread();

            void endFlexCounterThread();

            void flexCounterThreadRunFunction();

        private:

            typedef void (FlexCounter::*collect_counters_handler_t)(
                    _In_ swss::Table &countersTable);

            typedef std::unordered_map<std::string, collect_counters_handler_t> collect_counters_handler_unordered_map_t;

        private: // collect counters:

            void collectPortCounters(
                    _In_ swss::Table &countersTable);

            void collectPortDebugCounters(
                    _In_ swss::Table &countersTable);

            void collectQueueCounters(
                    _In_ swss::Table &countersTable);

            void collectPriorityGroupCounters(
                    _In_ swss::Table &countersTable);

            void collectRifCounters(
                    _In_ swss::Table &countersTable);

            void collectBufferPoolCounters(
                    _In_ swss::Table &countersTable);

            void collectSwitchDebugCounters(
                    _In_ swss::Table &countersTable);

            void collectTunnelCounters(
                    _In_ swss::Table &countersTable);

            void collectFlowCounters(
                    _In_ swss::Table &countersTable);

        private: // collect attributes

            void collectQueueAttrs(
                    _In_ swss::Table &countersTable);

            void collectPriorityGroupAttrs(
                    _In_ swss::Table &countersTable);

            void collectMACsecSAAttrs(
                    _In_ swss::Table &countersTable);

            void collectAclCounterAttrs(
                    _In_ swss::Table &countersTable);

        private:

            void addCollectCountersHandler(
                    _In_ const std::string& key,
                    _In_ const collect_counters_handler_t &handler);

            void removeCollectCountersHandler(
                    _In_ const std::string& key);

        private:
            void waitPoll();

            void notifyPoll();

        private: // plugins

            std::set<std::string> m_queuePlugins;
            std::set<std::string> m_portPlugins;
            std::set<std::string> m_rifPlugins;
            std::set<std::string> m_priorityGroupPlugins;
            std::set<std::string> m_bufferPoolPlugins;
            std::set<std::string> m_tunnelPlugins;
            std::set<std::string> m_flowCounterPlugins;

        private: // supported counters

            std::set<sai_port_stat_t> m_supportedPortCounters;
            std::set<sai_ingress_priority_group_stat_t> m_supportedPriorityGroupCounters;
            std::set<sai_queue_stat_t> m_supportedQueueCounters;
            std::set<sai_router_interface_stat_t> m_supportedRifCounters;
            std::set<sai_buffer_pool_stat_t> m_supportedBufferPoolCounters;
            std::set<sai_tunnel_stat_t> m_supportedTunnelCounters;
            std::set<sai_counter_stat_t> m_supportedFlowCounters;

        private: // registered VID maps

            std::map<sai_object_id_t, std::shared_ptr<PortCounterIds>> m_portCounterIdsMap;
            std::map<sai_object_id_t, std::shared_ptr<PortCounterIds>> m_portDebugCounterIdsMap;
            std::map<sai_object_id_t, std::shared_ptr<QueueCounterIds>> m_queueCounterIdsMap;
            std::map<sai_object_id_t, std::shared_ptr<IngressPriorityGroupCounterIds>> m_priorityGroupCounterIdsMap;
            std::map<sai_object_id_t, std::shared_ptr<RifCounterIds>> m_rifCounterIdsMap;
            std::map<sai_object_id_t, std::shared_ptr<BufferPoolCounterIds>> m_bufferPoolCounterIdsMap;
            std::map<sai_object_id_t, std::shared_ptr<SwitchCounterIds>> m_switchDebugCounterIdsMap;
            std::map<sai_object_id_t, std::shared_ptr<TunnelCounterIds>> m_tunnelCounterIdsMap;
            std::map<sai_object_id_t, std::shared_ptr<FlowCounterIds>> m_flowCounterIdsMap;

            std::map<sai_object_id_t, std::shared_ptr<QueueAttrIds>> m_queueAttrIdsMap;
            std::map<sai_object_id_t, std::shared_ptr<IngressPriorityGroupAttrIds>> m_priorityGroupAttrIdsMap;
            std::map<sai_object_id_t, std::shared_ptr<MACsecSAAttrIds>> m_macsecSAAttrIdsMap;
            std::map<sai_object_id_t, std::shared_ptr<AclCounterAttrIds>> m_aclCounterAttrIdsMap;

        private:

            bool m_runFlexCounterThread;

            std::shared_ptr<std::thread> m_flexCounterThread;

            std::mutex m_mtxSleep;

            std::condition_variable m_cvSleep;

            std::mutex m_mtx;

            std::condition_variable m_pollCond;

            bool m_readyToPoll;

            uint32_t m_pollInterval;

            std::string m_instanceId;

            sai_stats_mode_t m_statsMode;

            bool m_enable;

            collect_counters_handler_unordered_map_t m_collectCountersHandlers;

            std::shared_ptr<sairedis::SaiInterface> m_vendorSai;

            std::string m_dbCounters;

            bool m_isDiscarded;
    };
}
