#include "syncd_pfc_watchdog.h"
#include "syncd.h"

#define PFC_WD_POLL_MSECS 50

PfcWatchdog::PfcCounterIds::PfcCounterIds(
        _In_ sai_object_id_t queue,
        _In_ sai_object_id_t port,
        _In_ const std::vector<sai_port_stat_t> &portIds,
        _In_ const std::vector<sai_queue_stat_t> &queueIds):
    queueId(queue), portId(port), portCounterIds(portIds), queueCounterIds(queueIds)
{
}

void PfcWatchdog::setPortCounterList(
        _In_ sai_object_id_t queueVid,
        _In_ sai_object_id_t queueId,
        _In_ const std::vector<sai_port_stat_t> &counterIds)
{
    SWSS_LOG_ENTER();

    PfcWatchdog &wd = getInstance();

    sai_object_id_t portId = queueIdToPortId(queueId);
    if (portId == SAI_NULL_OBJECT_ID)
    {
        return;
    }

    auto it = wd.m_counterIdsMap.find(queueVid);
    if (it != wd.m_counterIdsMap.end())
    {
        (*it).second->portCounterIds = counterIds;
        return;
    }

    auto pfcCounterIds = std::make_shared<PfcCounterIds>(queueId,
                                                         portId,
                                                         counterIds,
                                                         std::vector<sai_queue_stat_t>());
    wd.m_counterIdsMap.emplace(queueVid, pfcCounterIds);

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

    sai_object_id_t portId = queueIdToPortId(queueId);
    if (portId == SAI_NULL_OBJECT_ID)
    {
        return;
    }

    auto it = wd.m_counterIdsMap.find(queueVid);
    if (it != wd.m_counterIdsMap.end())
    {
        (*it).second->queueCounterIds = counterIds;
        return;
    }

    auto pfcCounterIds = std::make_shared<PfcCounterIds>(queueId,
                                                        portId,
                                                        std::vector<sai_port_stat_t>(),
                                                        counterIds);
    wd.m_counterIdsMap.emplace(queueVid, pfcCounterIds);

    // Start watchdog thread in case it was not running due to empty counter IDs map
    wd.startWatchdogThread();
}

void PfcWatchdog::removeQueue(
        _In_ sai_object_id_t queueVid)
{
    SWSS_LOG_ENTER();

    PfcWatchdog &wd = getInstance();

    auto it = wd.m_counterIdsMap.find(queueVid);
    if (it == wd.m_counterIdsMap.end())
    {
        SWSS_LOG_ERROR("Trying to remove nonexisting queue counter Ids 0x%lx", queueVid);
        return;
    }

    wd.m_counterIdsMap.erase(it);

    // Stop watchdog thread if counter IDs map is empty
    if (wd.m_counterIdsMap.empty())
    {
        wd.endWatchdogThread();
    }
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

sai_object_id_t PfcWatchdog::queueIdToPortId(
        _In_ sai_object_id_t queueId)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr =
    {
        .id = SAI_QUEUE_ATTR_PORT,
        .value =
        {
            .oid = queueId,
        }
    };

    sai_status_t status = sai_metadata_sai_queue_api->get_queue_attribute(queueId, 1, &attr);
    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("Failed to get port Id of queue 0x%lx: %d", queueId, status);
        return SAI_NULL_OBJECT_ID;
    }

    return attr.value.oid;
}

void PfcWatchdog::collectCounters(
        _In_ swss::Table &countersTable)
{
    SWSS_LOG_ENTER();

    std::lock_guard<std::mutex> lock(g_mutex);

    // Collect stats for every registered queue
    for (const auto &kv: m_counterIdsMap)
    {
        const auto &queueVid = kv.first;
        const auto &queueId = kv.second->queueId;
        const auto &portId = kv.second->portId;
        const auto &portCounterIds = kv.second->portCounterIds;
        const auto &queueCounterIds = kv.second->queueCounterIds;

        std::vector<uint64_t> portStats(portCounterIds.size());
        std::vector<uint64_t> queueStats(queueCounterIds.size());

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

        // Get queue stats
        status = sai_metadata_sai_queue_api->get_queue_stats(
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

        for (size_t i = 0; i != portCounterIds.size(); i++)
        {
            const std::string &counterName = sai_serialize_port_stat(portCounterIds[i]);
            values.emplace_back(counterName, std::to_string(portStats[i]));
        }

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

void PfcWatchdog::pfcWatchdogThread(void)
{
    SWSS_LOG_ENTER();

    swss::DBConnector db(COUNTERS_DB, swss::DBConnector::DEFAULT_UNIXSOCKET, 0);
    swss::Table countersTable(&db, COUNTERS_TABLE);

    while (m_runPfcWatchdogThread)
    {
        collectCounters(countersTable);

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
