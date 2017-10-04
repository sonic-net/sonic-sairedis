#ifndef PFC_WATCHDOG_H
#define PFC_WATCHDOG_H

extern "C" {
#include "sai.h"
}

#include <atomic>
#include <vector>
#include <set>
#include <condition_variable>
#include "swss/table.h"

class PfcWatchdog
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

        PfcWatchdog(
                _In_ const PfcWatchdog&) = delete;
        ~PfcWatchdog(void);

    private:
        struct QueueCounterIds
        {
            QueueCounterIds(
                    _In_ sai_object_id_t queue,
                    _In_ const std::vector<sai_queue_stat_t> &queueIds);

            sai_object_id_t queueId;
            std::vector<sai_queue_stat_t> queueCounterIds;
        };

        struct PortCounterIds
        {
            PortCounterIds(
                    _In_ sai_object_id_t port,
                    _In_ const std::vector<sai_port_stat_t> &portIds);

            sai_object_id_t portId;
            std::vector<sai_port_stat_t> portCounterIds;
        };

        PfcWatchdog(void);
        static PfcWatchdog& getInstance(void);
        void collectCounters(
                _In_ swss::Table &countersTable);
        void runPlugins(
                _In_ swss::DBConnector& db);
        void pfcWatchdogThread(void);
        void startWatchdogThread(void);
        void endWatchdogThread(void);

        // Key is a Virtual ID
        std::map<sai_object_id_t, std::shared_ptr<PortCounterIds>> m_portCounterIdsMap;
        std::map<sai_object_id_t, std::shared_ptr<QueueCounterIds>> m_queueCounterIdsMap;

        // Plugins
        std::set<std::string> m_queuePlugins;
        std::set<std::string> m_portPlugins;

        std::atomic_bool m_runPfcWatchdogThread = { false };
        std::shared_ptr<std::thread> m_pfcWatchdogThread = nullptr;
        std::mutex m_mtxSleep;
        std::condition_variable m_cvSleep;
};

#endif
