#include "syncd_pfc_watchdog.h"
#include "syncd.h"
#include "swss/redisapi.h"

#define PFC_WD_POLL_MSECS 100

void on_queue_deadlock(
        _In_ uint32_t count,
        _In_ sai_queue_deadlock_notification_data_t *data);

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

void PfcWatchdog::runChecks(
        _In_ swss::DBConnector& db,
        _In_ std::string detectSha,
        _In_ std::string restoreSha)
{
    std::vector<std::string> argv = 
    {
        std::to_string(COUNTERS_DB),
        COUNTERS_TABLE,
        std::to_string(PFC_WD_POLL_MSECS * 1000)
    };

    std::vector<std::string> queueList;
    queueList.reserve(m_counterIdsMap.size());
    for (const auto& kv : m_counterIdsMap)
    {
        queueList.push_back(sai_serialize_object_id(kv.first));
    }

    auto stormCheckReply = runRedisScript(db, detectSha, queueList, argv);
    auto restoreCheckReply = runRedisScript(db, restoreSha, queueList, argv);

    std::vector<sai_queue_deadlock_notification_data_t> ntfData;

    for (const auto queueStr : stormCheckReply)
    {
        SWSS_LOG_ERROR("FOUND %s stormed", queueStr.c_str());
        sai_object_id_t queueVid;
        sai_deserialize_object_id(queueStr, queueVid);
        sai_queue_deadlock_notification_data_t data =
        {
            .queue_id = m_counterIdsMap[queueVid]->queueId,
            .event = SAI_QUEUE_PFC_DEADLOCK_EVENT_TYPE_DETECTED,
        };
        ntfData.push_back(data);
    }

    for (const auto queueStr : restoreCheckReply)
    {
        SWSS_LOG_ERROR("FOUND %s restored", queueStr.c_str());
        sai_object_id_t queueVid;
        sai_deserialize_object_id(queueStr, queueVid);
        sai_queue_deadlock_notification_data_t data =
        {
            .queue_id = m_counterIdsMap[queueVid]->queueId,
            .event = SAI_QUEUE_PFC_DEADLOCK_EVENT_TYPE_RECOVERED,
        };
        ntfData.push_back(data);
    }

    if (ntfData.size() != 0)
    {
        on_queue_deadlock(
                static_cast<uint32_t>(ntfData.size()),
                ntfData.data());
    }
}

void PfcWatchdog::pfcWatchdogThread(void)
{
    SWSS_LOG_ENTER();

    swss::DBConnector db(COUNTERS_DB, swss::DBConnector::DEFAULT_UNIXSOCKET, 0);
    swss::Table countersTable(&db, COUNTERS_TABLE);

    std::string platform = getenv("platform") ? getenv("platform") : "";

    if (platform == "")
    {
        SWSS_LOG_ERROR("Environment variable 'platform' is not defined");
        return;
    }

    std::string detectScriptName = "pfc_detect_" + platform + ".lua";
    std::string restoreScriptName = "pfc_restore_" + platform + ".lua";

    bool checkQueues = false;
    std::string detectSha;
    std::string restoreSha;

    try
    {
        // Load script for storm detection
        std::string detectLuaScript = swss::loadLuaScript(detectScriptName);
        detectSha = swss::loadRedisScript(&db, detectLuaScript);

        // Load script for restoration check
        std::string restoreLuaScript = swss::loadLuaScript(restoreScriptName);
        restoreSha = swss::loadRedisScript(&db, restoreLuaScript);

        checkQueues = true;
    }
    catch(...)
    {
        SWSS_LOG_WARN("Lua scripts for PFC watchdog were not loaded");
    }

    while (m_runPfcWatchdogThread)
    {
        collectCounters(countersTable);
        if (checkQueues)
        {
            runChecks(db, detectSha, restoreSha);
        }

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
