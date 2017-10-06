#include "syncd_pfc_watchdog.h"
#include "syncd.h"
#include "swss/redisapi.h"

#define PFC_WD_POLL_MSECS 100

PfcWatchdog::PortCounterIds::PortCounterIds(
        _In_ sai_object_id_t port,
        _In_ const std::vector<sai_port_stat_t> &portIds):
    portId(port), portCounterIds(portIds)
{
}

PfcWatchdog::QueueCounterIds::QueueCounterIds(
        _In_ sai_object_id_t queue,
        _In_ const std::vector<sai_queue_stat_t> &queueIds):
    queueId(queue), queueCounterIds(queueIds)
{
}

void PfcWatchdog::setPortCounterList(
        _In_ sai_object_id_t portVid,
        _In_ sai_object_id_t portId,
        _In_ const std::vector<sai_port_stat_t> &counterIds)
{
    SWSS_LOG_ENTER();

    PfcWatchdog &wd = getInstance();

    auto it = wd.m_portCounterIdsMap.find(portVid);
    if (it != wd.m_portCounterIdsMap.end())
    {
        (*it).second->portCounterIds = counterIds;
        return;
    }

    auto portCounterIds = std::make_shared<PortCounterIds>(portId, counterIds);
    wd.m_portCounterIdsMap.emplace(portVid, portCounterIds);

    // Start watchdog thread in case it was not running due to empty counter IDs map
    wd.startWatchdogThread();
}

void PfcWatchdog::setQueueCounterList(
        _In_ sai_object_id_t queueVid,
        _In_ sai_object_id_t queueId,
        _In_ const std::vector<sai_queue_stat_t> &counterIds)
{
    SWSS_LOG_ENTER();

    PfcWatchdog &wd = getInstance();

    auto it = wd.m_queueCounterIdsMap.find(queueVid);
    if (it != wd.m_queueCounterIdsMap.end())
    {
        (*it).second->queueCounterIds = counterIds;
        return;
    }

    auto queueCounterIds = std::make_shared<QueueCounterIds>(queueId, counterIds);
    wd.m_queueCounterIdsMap.emplace(queueVid, queueCounterIds);

    // Start watchdog thread in case it was not running due to empty counter IDs map
    wd.startWatchdogThread();
}

void PfcWatchdog::removePort(
        _In_ sai_object_id_t portVid)
{
    SWSS_LOG_ENTER();

    PfcWatchdog &wd = getInstance();

    auto it = wd.m_portCounterIdsMap.find(portVid);
    if (it == wd.m_portCounterIdsMap.end())
    {
        SWSS_LOG_ERROR("Trying to remove nonexisting port counter Ids 0x%lx", portVid);
        return;
    }

    wd.m_portCounterIdsMap.erase(it);

    // Stop watchdog thread if counter IDs map is empty
    if (wd.m_queueCounterIdsMap.empty() && wd.m_portCounterIdsMap.empty())
    {
        wd.endWatchdogThread();
    }
}

void PfcWatchdog::removeQueue(
        _In_ sai_object_id_t queueVid)
{
    SWSS_LOG_ENTER();

    PfcWatchdog &wd = getInstance();

    auto it = wd.m_queueCounterIdsMap.find(queueVid);
    if (it == wd.m_queueCounterIdsMap.end())
    {
        SWSS_LOG_ERROR("Trying to remove nonexisting queue counter Ids 0x%lx", queueVid);
        return;
    }

    wd.m_queueCounterIdsMap.erase(it);

    // Stop watchdog thread if counter IDs map is empty
    if (wd.m_queueCounterIdsMap.empty() && wd.m_portCounterIdsMap.empty())
    {
        wd.endWatchdogThread();
    }
}

void PfcWatchdog::addPortCounterPlugin(
        _In_ std::string sha)
{
    SWSS_LOG_ENTER();

    PfcWatchdog &wd = getInstance();

    if (wd.m_portPlugins.find(sha) != wd.m_portPlugins.end() ||
            wd.m_queuePlugins.find(sha) != wd.m_queuePlugins.end())
    {
        SWSS_LOG_ERROR("Plugin %s already registered", sha.c_str());
    }

    wd.m_portPlugins.insert(sha);
    SWSS_LOG_NOTICE("Port counters plugin %s registered", sha.c_str());
}

void PfcWatchdog::addQueueCounterPlugin(
        _In_ std::string sha)
{
    SWSS_LOG_ENTER();

    PfcWatchdog &wd = getInstance();

    if (wd.m_portPlugins.find(sha) != wd.m_portPlugins.end() ||
            wd.m_queuePlugins.find(sha) != wd.m_queuePlugins.end())
    {
        SWSS_LOG_ERROR("Plugin %s already registered", sha.c_str());
    }

    wd.m_queuePlugins.insert(sha);
    SWSS_LOG_NOTICE("Queue counters plugin %s registered", sha.c_str());
}

void PfcWatchdog::removeCounterPlugin(
        _In_ std::string sha)
{
    SWSS_LOG_ENTER();

    PfcWatchdog &wd = getInstance();

    wd.m_queuePlugins.erase(sha);
    wd.m_portPlugins.erase(sha);
}

PfcWatchdog::~PfcWatchdog(void)
{
    endWatchdogThread();
}

PfcWatchdog::PfcWatchdog(void)
{
}

PfcWatchdog& PfcWatchdog::getInstance(void)
{
    static PfcWatchdog wd;

    return wd;
}

void PfcWatchdog::collectCounters(
        _In_ swss::Table &countersTable)
{
    SWSS_LOG_ENTER();

    std::lock_guard<std::mutex> lock(g_mutex);

    // Collect stats for every registered port
    for (const auto &kv: m_portCounterIdsMap)
    {
        const auto &portVid = kv.first;
        const auto &portId = kv.second->portId;
        const auto &portCounterIds = kv.second->portCounterIds;

        std::vector<uint64_t> portStats(portCounterIds.size());

        // Get port stats for queue
        sai_status_t status = sai_metadata_sai_port_api->get_port_stats(
                portId,
                static_cast<uint32_t>(portCounterIds.size()),
                portCounterIds.data(),
                portStats.data());
        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("Failed to get stats of port 0x%lx: %d", portId, status);
            continue;
        }

        // Push all counter values to a single vector
        std::vector<swss::FieldValueTuple> values;

        for (size_t i = 0; i != portCounterIds.size(); i++)
        {
            const std::string &counterName = sai_serialize_port_stat(portCounterIds[i]);
            values.emplace_back(counterName, std::to_string(portStats[i]));
        }

        // Write counters to DB
        std::string portVidStr = sai_serialize_object_id(portVid);

        countersTable.set(portVidStr, values, "");
    }

    // Collect stats for every registered queue
    for (const auto &kv: m_queueCounterIdsMap)
    {
        const auto &queueVid = kv.first;
        const auto &queueId = kv.second->queueId;
        const auto &queueCounterIds = kv.second->queueCounterIds;

        std::vector<uint64_t> queueStats(queueCounterIds.size());

        // Get queue stats
        sai_status_t status = sai_metadata_sai_queue_api->get_queue_stats(
                queueId,
                static_cast<uint32_t>(queueCounterIds.size()),
                queueCounterIds.data(),
                queueStats.data());
        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("Failed to get stats of queue 0x%lx: %d", queueVid, status);
            continue;
        }

        // Push all counter values to a single vector
        std::vector<swss::FieldValueTuple> values;

        for (size_t i = 0; i != queueCounterIds.size(); i++)
        {
            const std::string &counterName = sai_serialize_queue_stat(queueCounterIds[i]);
            values.emplace_back(counterName, std::to_string(queueStats[i]));
        }

        // Write counters to DB
        std::string queueVidStr = sai_serialize_object_id(queueVid);

        countersTable.set(queueVidStr, values, "");
    }
}

void PfcWatchdog::runPlugins(
        _In_ swss::DBConnector& db)
{
    SWSS_LOG_ENTER();

    std::lock_guard<std::mutex> lock(g_mutex);

    const std::vector<std::string> argv = 
    {
        std::to_string(COUNTERS_DB),
        COUNTERS_TABLE,
        std::to_string(PFC_WD_POLL_MSECS * 1000)
    };

    std::vector<std::string> portList;
    portList.reserve(m_portCounterIdsMap.size());
    for (const auto& kv : m_portCounterIdsMap)
    {
        portList.push_back(sai_serialize_object_id(kv.first));
    }

    for (const auto& sha : m_portPlugins)
    {
        runRedisScript(db, sha, portList, argv);
    }

    std::vector<std::string> queueList;
    queueList.reserve(m_queueCounterIdsMap.size());
    for (const auto& kv : m_queueCounterIdsMap)
    {
        queueList.push_back(sai_serialize_object_id(kv.first));
    }

    for (const auto& sha : m_queuePlugins)
    {
        runRedisScript(db, sha, queueList, argv);
    }
}

void PfcWatchdog::pfcWatchdogThread(void)
{
    SWSS_LOG_ENTER();

    swss::DBConnector db(COUNTERS_DB, swss::DBConnector::DEFAULT_UNIXSOCKET, 0);
    swss::Table countersTable(&db, COUNTERS_TABLE);

    while (m_runPfcWatchdogThread)
    {
        collectCounters(countersTable);
        runPlugins(db);

        std::unique_lock<std::mutex> lk(m_mtxSleep);
        m_cvSleep.wait_for(lk, std::chrono::milliseconds(PFC_WD_POLL_MSECS));
    }
}

void PfcWatchdog::startWatchdogThread(void)
{
    SWSS_LOG_ENTER();

    if (m_runPfcWatchdogThread.load() == true)
    {
        return;
    }

    m_runPfcWatchdogThread = true;

    m_pfcWatchdogThread = std::make_shared<std::thread>(&PfcWatchdog::pfcWatchdogThread, this);
    
    SWSS_LOG_INFO("PFC Watchdog thread started");
}

void PfcWatchdog::endWatchdogThread(void)
{
    SWSS_LOG_ENTER();

    if (m_runPfcWatchdogThread.load() == false)
    {
        return;
    }

    m_runPfcWatchdogThread = false;

    m_cvSleep.notify_all();

    if (m_pfcWatchdogThread != nullptr)
    {
        SWSS_LOG_INFO("Wait for PFC Watchdog thread to end");

        m_pfcWatchdogThread->join();
    }

    SWSS_LOG_INFO("PFC Watchdog thread ended");
}
