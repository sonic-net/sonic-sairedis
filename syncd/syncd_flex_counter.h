#ifndef FLEX_COUNTER_H
#define FLEX_COUNTER_H

extern "C" {
#include "sai.h"
}

#include <atomic>
#include <vector>
#include <set>
#include <condition_variable>
#include "swss/table.h"

class FlexCounter
{
    public:
        static void setPollInterval(
                _In_ uint32_t pollInterval,
                _In_ std::string instanceId);
        static void setPortCounterList(
                _In_ sai_object_id_t portVid,
                _In_ sai_object_id_t portId,
                _In_ std::string instanceId,
                _In_ const std::vector<sai_port_stat_t> &counterIds);
        static void setQueueCounterList(
                _In_ sai_object_id_t queueVid,
                _In_ sai_object_id_t queueId,
                _In_ std::string instanceId,
                _In_ const std::vector<sai_queue_stat_t> &counterIds);
        static void setPriorityGroupCounterList(
                _In_ sai_object_id_t priorityGroupVid,
                _In_ sai_object_id_t priorityGroupId,
                _In_ std::string instanceId,
                _In_ const std::vector<sai_ingress_priority_group_stat_t> &counterIds);
        static void setRifCounterList(
                _In_ sai_object_id_t rifVid,
                _In_ sai_object_id_t rifId,
                _In_ std::string instanceId,
                _In_ const std::vector<sai_router_interface_stat_t> &counterIds);
        static void setQueueAttrList(
                _In_ sai_object_id_t queueVid,
                _In_ sai_object_id_t queueId,
                _In_ std::string instanceId,
                _In_ const std::vector<sai_queue_attr_t> &attrIds);
        static void setPriorityGroupAttrList(
                _In_ sai_object_id_t priorityGroupVid,
                _In_ sai_object_id_t priorityGroupId,
                _In_ std::string instanceId,
                _In_ const std::vector<sai_ingress_priority_group_attr_t> &attrIds);
        static void updateFlexCounterStatus(
                _In_ std::string status,
                _In_ std::string instanceId);
        static void updateFlexCounterStatsMode(
                 _In_ std::string mode,
                 _In_ std::string instanceId);

        static void removePort(
                _In_ sai_object_id_t portVid,
                _In_ std::string instanceId);
        static void removeQueue(
                _In_ sai_object_id_t queueVid,
                _In_ std::string instanceId);
        static void removePriorityGroup(
                _In_ sai_object_id_t priorityGroupVid,
                _In_ std::string instanceId);
        static void removeRif(
                _In_ sai_object_id_t rifVid,
                _In_ std::string instanceId);

        static void addPortCounterPlugin(
                _In_ std::string sha,
                _In_ std::string instanceId);
        static void addQueueCounterPlugin(
                _In_ std::string sha,
                _In_ std::string instanceId);
        static void addPriorityGroupCounterPlugin(
                _In_ std::string sha,
                _In_ std::string instanceId);
        static void removeCounterPlugin(
                _In_ std::string sha,
                _In_ std::string instanceId);
        static void removeCounterPlugin(
                _In_ std::string instanceId);
        static void removeAllCounters();

        FlexCounter(
                _In_ const FlexCounter&) = delete;
        ~FlexCounter(void);

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

        struct PortCounterIds
        {
            PortCounterIds(
                    _In_ sai_object_id_t port,
                    _In_ const std::vector<sai_port_stat_t> &portIds);

            sai_object_id_t portId;
            std::vector<sai_port_stat_t> portCounterIds;
        };

        struct RifCounterIds
        {
            RifCounterIds(
                    _In_ sai_object_id_t rif,
                    _In_ const std::vector<sai_router_interface_stat_t> &rifIds);

            sai_object_id_t rifId;
            std::vector<sai_router_interface_stat_t> rifCounterIds;
        };

        FlexCounter(std::string instanceId);
        static FlexCounter& getInstance(std::string instanceId);
        static void removeInstance(std::string instanceId);

        void collectCounters(_In_ swss::Table &countersTable);
        void runPlugins(_In_ swss::DBConnector& db);
        void flexCounterThread(void);
        void startFlexCounterThread(void);
        void endFlexCounterThread(void);

        void saiUpdateSupportedPortCounters(sai_object_id_t portId);
        void saiUpdateSupportedQueueCounters(sai_object_id_t queueId, const std::vector<sai_queue_stat_t> &counterIds);
        void saiUpdateSupportedPriorityGroupCounters(sai_object_id_t priorityGroupId, const std::vector<sai_ingress_priority_group_stat_t> &counterIds);
        void saiUpdateSupportedRifCounters(sai_object_id_t rifId);
        bool isPortCounterSupported(sai_port_stat_t counter) const;
        bool isQueueCounterSupported(sai_queue_stat_t counter) const;
        bool isPriorityGroupCounterSupported(sai_ingress_priority_group_stat_t counter) const;
        bool isRifCounterSupported(sai_router_interface_stat_t counter) const;
        bool isEmpty();

        // Key is a Virtual ID
        std::map<sai_object_id_t, std::shared_ptr<PortCounterIds>> m_portCounterIdsMap;
        std::map<sai_object_id_t, std::shared_ptr<QueueCounterIds>> m_queueCounterIdsMap;
        std::map<sai_object_id_t, std::shared_ptr<QueueAttrIds>> m_queueAttrIdsMap;
        std::map<sai_object_id_t, std::shared_ptr<IngressPriorityGroupCounterIds>> m_priorityGroupCounterIdsMap;
        std::map<sai_object_id_t, std::shared_ptr<IngressPriorityGroupAttrIds>> m_priorityGroupAttrIdsMap;
        std::map<sai_object_id_t, std::shared_ptr<RifCounterIds>> m_rifCounterIdsMap;

        // Plugins
        std::set<std::string> m_queuePlugins;
        std::set<std::string> m_portPlugins;
        std::set<std::string> m_priorityGroupPlugins;

        std::atomic_bool m_runFlexCounterThread = { false };
        std::shared_ptr<std::thread> m_flexCounterThread = nullptr;
        std::mutex m_mtxSleep;
        std::condition_variable m_cvSleep;

        uint32_t m_pollInterval = 0;
        std::string m_instanceId;
        sai_stats_mode_t m_statsMode;
        bool m_enable = false;
};

#endif
