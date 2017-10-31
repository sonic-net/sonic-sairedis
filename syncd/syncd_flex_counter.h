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
        static void setPortCounterList(
                _In_ sai_object_id_t portVid,
                _In_ sai_object_id_t portId,
                _In_ const std::vector<sai_port_stat_t> &counterIds);
        static void setQueueCounterList(
                _In_ sai_object_id_t queueVid,
                _In_ sai_object_id_t queueId,
                _In_ const std::vector<sai_queue_stat_t> &counterIds);
        static void setQueueAttrList(
                _In_ sai_object_id_t queueVid,
                _In_ sai_object_id_t queueId,
                _In_ const std::vector<sai_queue_attr_t> &attrIds);

        static void removePort(
                _In_ sai_object_id_t portVid);
        static void removeQueue(
                _In_ sai_object_id_t queueVid);

        static void addPortCounterPlugin(
                _In_ std::string sha);
        static void addQueueCounterPlugin(
                _In_ std::string sha);
        static void removeCounterPlugin(
                _In_ std::string sha);

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

        struct PortCounterIds
        {
            PortCounterIds(
                    _In_ sai_object_id_t port,
                    _In_ const std::vector<sai_port_stat_t> &portIds);

            sai_object_id_t portId;
            std::vector<sai_port_stat_t> portCounterIds;
        };

        FlexCounter(void);
        static FlexCounter& getInstance(void);
        void collectCounters(
                _In_ swss::Table &countersTable);
        void runPlugins(
                _In_ swss::DBConnector& db);
        void flexCounterThread(void);
        void startFlexCounterThread(void);
        void endFlexCounterThread(void);

        // Key is a Virtual ID
        std::map<sai_object_id_t, std::shared_ptr<PortCounterIds>> m_portCounterIdsMap;
        std::map<sai_object_id_t, std::shared_ptr<QueueCounterIds>> m_queueCounterIdsMap;
        std::map<sai_object_id_t, std::shared_ptr<QueueAttrIds>> m_queueAttrIdsMap;

        // Plugins
        std::set<std::string> m_queuePlugins;
        std::set<std::string> m_portPlugins;

        std::atomic_bool m_runFlexCounterThread = { false };
        std::shared_ptr<std::thread> m_flexCounterThread = nullptr;
        std::mutex m_mtxSleep;
        std::condition_variable m_cvSleep;
};

#endif
