#include "syncd_flex_counter.h"
#include "syncd.h"
#include "swss/redisapi.h"

/* Global map with FlexCounter instances for different polling interval */
static std::map<uint32_t, std::shared_ptr<FlexCounter>> g_flex_counters_map;


FlexCounter::PortCounterIds::PortCounterIds(
        _In_ sai_object_id_t port,
        _In_ const std::vector<sai_port_stat_t> &portIds):
    portId(port), portCounterIds(portIds)
{
}

FlexCounter::QueueCounterIds::QueueCounterIds(
        _In_ sai_object_id_t queue,
        _In_ const std::vector<sai_queue_stat_t> &queueIds):
    queueId(queue), queueCounterIds(queueIds)
{
}

FlexCounter::QueueAttrIds::QueueAttrIds(
        _In_ sai_object_id_t queue,
        _In_ const std::vector<sai_queue_attr_t> &queueIds):
    queueId(queue), queueAttrIds(queueIds)
{
}

/* The current implementation of 'setPortCounterList' and 'setQueueCounterList' are
 * not the same. Need to refactor these two functions to have the similar logic.
 * Either the full SAI attributes are queried once, or each of the needed counters
 * will be queried when they are set.
 */
void FlexCounter::setPortCounterList(
        _In_ sai_object_id_t portVid,
        _In_ sai_object_id_t portId,
        _In_ uint32_t pollInterval,
        _In_ const std::vector<sai_port_stat_t> &counterIds)
{
    SWSS_LOG_ENTER();

    FlexCounter &fc = getInstance(pollInterval);

    // Initialize the supported counters list before setting
    if (fc.m_supportedPortCounters.size() == 0)
    {
        fc.saiUpdateSupportedPortCounters(portId);
    }

    // Remove unsupported counters
    std::vector<sai_port_stat_t> supportedIds;
    for(auto &counter : counterIds)
    {
        if (fc.isPortCounterSupported(counter))
        {
            supportedIds.push_back(counter);
        }
    }

    if (supportedIds.size() == 0)
    {
        SWSS_LOG_ERROR("Port %s does not has supported counters", sai_serialize_object_id(portId).c_str());

        // Remove flex counter if counter IDs map is empty
        if (fc.m_queueCounterIdsMap.empty() && fc.m_portCounterIdsMap.empty() && fc.m_queueAttrIdsMap.empty())
        {
            removeInstance(pollInterval);
        }

        return;
    }

    auto it = fc.m_portCounterIdsMap.find(portVid);
    if (it != fc.m_portCounterIdsMap.end())
    {
        (*it).second->portCounterIds = supportedIds;
        return;
    }

    auto portCounterIds = std::make_shared<PortCounterIds>(portId, supportedIds);
    fc.m_portCounterIdsMap.emplace(portVid, portCounterIds);

    // Start flex counter thread in case it was not running due to empty counter IDs map
    fc.startFlexCounterThread();
}

void FlexCounter::setQueueCounterList(
        _In_ sai_object_id_t queueVid,
        _In_ sai_object_id_t queueId,
        _In_ uint32_t pollInterval,
        _In_ const std::vector<sai_queue_stat_t> &counterIds)
{
    SWSS_LOG_ENTER();

    FlexCounter &fc = getInstance(pollInterval);

    fc.saiUpdateSupportedQueueCounters(queueId, counterIds);

    // Remove unsupported counters
    std::vector<sai_queue_stat_t> supportedIds;
    for(auto &counter : counterIds)
    {
        if (fc.isQueueCounterSupported(counter))
        {
            supportedIds.push_back(counter);
        }
    }

    if (supportedIds.size() == 0)
    {
        SWSS_LOG_ERROR("Queue %s does not has supported counters", sai_serialize_object_id(queueId).c_str());

        // Remove flex counter if counter IDs map is empty
        if (fc.m_queueCounterIdsMap.empty() && fc.m_portCounterIdsMap.empty() && fc.m_queueAttrIdsMap.empty())
        {
            removeInstance(pollInterval);
        }

        return;
    }

    // Check if queue is able to provide the statistic
    std::vector<uint64_t> queueStats(supportedIds.size());
    sai_status_t status = sai_metadata_sai_queue_api->get_queue_stats(
            queueId,
            static_cast<uint32_t>(supportedIds.size()),
            supportedIds.data(),
            queueStats.data());

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("Queue %s can't provide the statistic",  sai_serialize_object_id(queueId).c_str());

        // Remove flex counter if counter IDs map is empty
        if (fc.m_queueCounterIdsMap.empty() && fc.m_portCounterIdsMap.empty() && fc.m_queueAttrIdsMap.empty())
        {
            removeInstance(pollInterval);
        }

        return;
    }

    auto it = fc.m_queueCounterIdsMap.find(queueVid);
    if (it != fc.m_queueCounterIdsMap.end())
    {
        (*it).second->queueCounterIds = supportedIds;
        return;
    }

    auto queueCounterIds = std::make_shared<QueueCounterIds>(queueId, supportedIds);
    fc.m_queueCounterIdsMap.emplace(queueVid, queueCounterIds);

    // Start flex counter thread in case it was not running due to empty counter IDs map
    fc.startFlexCounterThread();
}

void FlexCounter::setQueueAttrList(
        _In_ sai_object_id_t queueVid,
        _In_ sai_object_id_t queueId,
        _In_ uint32_t pollInterval,
        _In_ const std::vector<sai_queue_attr_t> &attrIds)
{
    SWSS_LOG_ENTER();

    FlexCounter &fc = getInstance(pollInterval);

    auto it = fc.m_queueAttrIdsMap.find(queueVid);
    if (it != fc.m_queueAttrIdsMap.end())
    {
        (*it).second->queueAttrIds = attrIds;
        return;
    }

    auto queueAttrIds = std::make_shared<QueueAttrIds>(queueId, attrIds);
    fc.m_queueAttrIdsMap.emplace(queueVid, queueAttrIds);

    // Start flex counter thread in case it was not running due to empty counter IDs map
    fc.startFlexCounterThread();
}


void FlexCounter::removePort(
        _In_ sai_object_id_t portVid,
        _In_ uint32_t pollInterval)
{
    SWSS_LOG_ENTER();

    FlexCounter &fc = getInstance(pollInterval);

    auto it = fc.m_portCounterIdsMap.find(portVid);
    if (it == fc.m_portCounterIdsMap.end())
    {
        SWSS_LOG_ERROR("Trying to remove nonexisting port counter Ids 0x%lx", portVid);
        // Remove flex counter if counter IDs map is empty
        if (fc.m_queueCounterIdsMap.empty() && fc.m_portCounterIdsMap.empty() && fc.m_queueAttrIdsMap.empty())
        {
            removeInstance(pollInterval);
        }
        return;
    }

    fc.m_portCounterIdsMap.erase(it);

    // Remove flex counter if counter IDs map is empty
    if (fc.m_queueCounterIdsMap.empty() && fc.m_portCounterIdsMap.empty() && fc.m_queueAttrIdsMap.empty())
    {
        removeInstance(pollInterval);
    }
}

void FlexCounter::removeQueue(
        _In_ sai_object_id_t queueVid,
        _In_ uint32_t pollInterval)
{
    SWSS_LOG_ENTER();

    FlexCounter &fc = getInstance(pollInterval);

    auto counterIter = fc.m_queueCounterIdsMap.find(queueVid);
    if (counterIter == fc.m_queueCounterIdsMap.end())
    {
        SWSS_LOG_ERROR("Trying to remove nonexisting queue counter Ids 0x%lx", queueVid);
        if (fc.m_queueCounterIdsMap.empty() && fc.m_portCounterIdsMap.empty() && fc.m_queueAttrIdsMap.empty())
        {
            removeInstance(pollInterval);
        }
        return;
    }

    fc.m_queueCounterIdsMap.erase(counterIter);

    auto attrIter = fc.m_queueAttrIdsMap.find(queueVid);
    if (attrIter == fc.m_queueAttrIdsMap.end())
    {
        SWSS_LOG_ERROR("Trying to remove nonexisting queue attr Ids 0x%lx", queueVid);
        if (fc.m_queueCounterIdsMap.empty() && fc.m_portCounterIdsMap.empty() && fc.m_queueAttrIdsMap.empty())
        {
            removeInstance(pollInterval);
        }
        return;
    }

    fc.m_queueAttrIdsMap.erase(attrIter);

    // Remove flex counter if counter IDs map is empty
    if (fc.m_queueCounterIdsMap.empty() && fc.m_portCounterIdsMap.empty() && fc.m_queueAttrIdsMap.empty())
    {
        removeInstance(pollInterval);
    }
}

void FlexCounter::addPortCounterPlugin(
        _In_ std::string sha,
        _In_ uint32_t pollInterval)
{
    SWSS_LOG_ENTER();

    FlexCounter &fc = getInstance(pollInterval);

    if (fc.m_portPlugins.find(sha) != fc.m_portPlugins.end() ||
            fc.m_queuePlugins.find(sha) != fc.m_queuePlugins.end())
    {
        SWSS_LOG_ERROR("Plugin %s already registered", sha.c_str());
    }

    fc.m_portPlugins.insert(sha);
    SWSS_LOG_NOTICE("Port counters plugin %s registered", sha.c_str());
}

void FlexCounter::addQueueCounterPlugin(
        _In_ std::string sha,
        _In_ uint32_t pollInterval)
{
    SWSS_LOG_ENTER();

    FlexCounter &fc = getInstance(pollInterval);

    if (fc.m_portPlugins.find(sha) != fc.m_portPlugins.end() ||
            fc.m_queuePlugins.find(sha) != fc.m_queuePlugins.end())
    {
        SWSS_LOG_ERROR("Plugin %s already registered", sha.c_str());
    }

    fc.m_queuePlugins.insert(sha);
    SWSS_LOG_NOTICE("Queue counters plugin %s registered", sha.c_str());
}

void FlexCounter::removeCounterPlugin(
        _In_ std::string sha,
        _In_ uint32_t pollInterval)
{
    SWSS_LOG_ENTER();

    FlexCounter &fc = getInstance(pollInterval);

    fc.m_queuePlugins.erase(sha);
    fc.m_portPlugins.erase(sha);

    // Remove flex counter if counter IDs maps are empty
    if (fc.m_queueCounterIdsMap.empty() && fc.m_portCounterIdsMap.empty() && fc.m_queueAttrIdsMap.empty() &&
        fc.m_queuePlugins.empty() && fc.m_portPlugins.empty())
    {
        removeInstance(pollInterval);
    }
}

FlexCounter::~FlexCounter(void)
{
    endFlexCounterThread();
}

bool FlexCounter::isPortCounterSupported(sai_port_stat_t counter) const
{
    return m_supportedPortCounters.count(counter) != 0;
}

bool FlexCounter::isQueueCounterSupported(sai_queue_stat_t counter) const
{
    return m_supportedQueueCounters.count(counter) != 0;
}

FlexCounter::FlexCounter(uint32_t pollInterval) : m_pollInterval(pollInterval)
{
}

FlexCounter& FlexCounter::getInstance(uint32_t pollInterval)
{
    if (g_flex_counters_map.count(pollInterval) == 0)
    {
        std::shared_ptr<FlexCounter> instance(new FlexCounter(pollInterval));
        g_flex_counters_map.emplace(pollInterval, instance);
    }

    return *(g_flex_counters_map[pollInterval]);
}

void FlexCounter::removeInstance(uint32_t pollInterval)
{
    g_flex_counters_map.erase(pollInterval);
}

void FlexCounter::collectCounters(
        _In_ swss::Table &countersTable)
{
    SWSS_LOG_ENTER();

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

    // Collect attrs for every registered queue
    for (const auto &kv: m_queueAttrIdsMap)
    {
        const auto &queueVid = kv.first;
        const auto &queueId = kv.second->queueId;
        const auto &queueAttrIds = kv.second->queueAttrIds;

        std::vector<sai_attribute_t> queueAttr(queueAttrIds.size());

        for (uint64_t i =0; i< queueAttrIds.size(); i++)
        {
            queueAttr[i].id = queueAttrIds[i];
        }

        // Get queue attr
        sai_status_t status = sai_metadata_sai_queue_api->get_queue_attribute(
                queueId,
                static_cast<uint32_t>(queueAttrIds.size()),
                queueAttr.data());

        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("Failed to get attr of queue 0x%lx: %d", queueVid, status);
            continue;
        }

        // Push all counter values to a single vector
        std::vector<swss::FieldValueTuple> values;

        for (size_t i = 0; i != queueAttrIds.size(); i++)
        {
            const std::string &counterName = sai_serialize_queue_attr(queueAttrIds[i]);
            auto meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_QUEUE, queueAttr[i].id);

            values.emplace_back(counterName, sai_serialize_attr_value(*meta, queueAttr[i]));
        }
        // Write counters to DB
        std::string queueVidStr = sai_serialize_object_id(queueVid);

        countersTable.set(queueVidStr, values, "");
    }

}

void FlexCounter::runPlugins(
        _In_ swss::DBConnector& db,
        _In_ uint32_t pollInterval)
{
    SWSS_LOG_ENTER();

    const std::vector<std::string> argv = 
    {
        std::to_string(COUNTERS_DB),
        COUNTERS_TABLE,
        std::to_string(pollInterval * 1000)
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

void FlexCounter::flexCounterThread(void)
{
    SWSS_LOG_ENTER();

    swss::DBConnector db(COUNTERS_DB, swss::DBConnector::DEFAULT_UNIXSOCKET, 0);
    swss::Table countersTable(&db, COUNTERS_TABLE);
    uint32_t correction = 0;

    while (m_runFlexCounterThread)
    {
        {
            auto start = std::chrono::steady_clock::now();
            std::lock_guard<std::mutex> lock(g_mutex);
            collectCounters(countersTable);
            auto finish = std::chrono::steady_clock::now();

            uint32_t delay = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(finish - start).count());
            uint32_t newCorrection = delay % m_pollInterval;

            // Run plugins with corrected interval
            // First we subtract correction from previous sleep and then add delay from current counters read
            runPlugins(db, m_pollInterval - correction + delay);
            correction = newCorrection;
            SWSS_LOG_ERROR("ELAPSED %d", std::chrono::duration_cast<std::chrono::milliseconds>(finish - start).count());
        }

        std::unique_lock<std::mutex> lk(m_mtxSleep);
        m_cvSleep.wait_for(lk, std::chrono::milliseconds(m_pollInterval - correction));
    }
}

void FlexCounter::startFlexCounterThread(void)
{
    SWSS_LOG_ENTER();

    if (m_runFlexCounterThread.load() == true)
    {
        return;
    }

    m_runFlexCounterThread = true;

    m_flexCounterThread = std::make_shared<std::thread>(&FlexCounter::flexCounterThread, this);
    
    SWSS_LOG_INFO("Flex Counter thread started");
}

void FlexCounter::endFlexCounterThread(void)
{
    SWSS_LOG_ENTER();

    if (m_runFlexCounterThread.load() == false)
    {
        return;
    }

    m_runFlexCounterThread = false;

    m_cvSleep.notify_all();

    if (m_flexCounterThread != nullptr)
    {
        SWSS_LOG_INFO("Wait for Flex Counter thread to end");

        m_flexCounterThread->join();
    }

    SWSS_LOG_INFO("Flex Counter thread ended");
}

void FlexCounter::saiUpdateSupportedPortCounters(sai_object_id_t portId)
{
    uint64_t value;
    for (int cntr_id = SAI_PORT_STAT_IF_IN_OCTETS; cntr_id <= SAI_PORT_STAT_PFC_7_ON2OFF_RX_PKTS; ++cntr_id)
    {
        sai_port_stat_t counter = static_cast<sai_port_stat_t>(cntr_id);

        sai_status_t status = sai_metadata_sai_port_api->get_port_stats(portId, 1, &counter, &value);

        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_WARN("Counter %s is not supported on port RID %s: %s",
                    sai_serialize_port_stat(counter).c_str(),
                    sai_serialize_object_id(portId).c_str(),
                    sai_serialize_status(status).c_str());

            continue;
        }

        m_supportedPortCounters.insert(counter);
    }
}

void FlexCounter::saiUpdateSupportedQueueCounters(
        _In_ sai_object_id_t queueId,
        _In_ const std::vector<sai_queue_stat_t> &counterIds)
{
    uint64_t value;
    m_supportedQueueCounters.clear();

    for (auto &counter : counterIds)
    {
        sai_status_t status = sai_metadata_sai_queue_api->get_queue_stats(queueId, 1, &counter, &value);

        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_WARN("Counter %s is not supported on queue %s, rv: %s",
                    sai_serialize_queue_stat(counter).c_str(),
                    sai_serialize_object_id(queueId).c_str(),
                    sai_serialize_status(status).c_str());

            continue;
        }
        else
        {
            m_supportedQueueCounters.insert(counter);
        }

    }
}
