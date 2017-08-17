#include <condition_variable>
#include <sstream>
#include "syncd.h"

void collectCounters(swss::Table &countersTable,
                     const std::vector<sai_port_stat_counter_t> &supportedCounters)
{
    // collect counters should be under mutex
    // sice configuration can change and we
    // don't want that during counters collection
    //
    // XXX currently lock for counters is disabled
    // since on new 0.9.4 can cause deadlock since
    // interanl sai implementaion also uses locks
    // std::lock_guard<std::mutex> lock(g_mutex);

    SWSS_LOG_ENTER();

    uint32_t countersSize = (uint32_t)supportedCounters.size();

    std::vector<uint64_t> counters;

    counters.resize(countersSize);

    auto ports = saiGetPortList();

    for (auto &portId: ports)
    {
        sai_status_t status = sai_port_api->get_port_stats(portId, supportedCounters.data(), countersSize, counters.data());

        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("failed to collect counters for port 0x%lx: %d", portId, status);
            return;
        }

        sai_object_id_t vid = translate_rid_to_vid(portId);

        std::string strPortId = sai_serialize_object_id(vid);

        // for counters, use port vid as printf "%llx" format
        std::stringstream ss;
        ss << std::hex << vid;
        strPortId = ss.str();

        std::vector<swss::FieldValueTuple> values;

        for (size_t idx = 0; idx < counters.size(); idx++)
        {
            const std::string &field = sai_serialize_port_stat(supportedCounters[idx]);
            const std::string &value = std::to_string(counters[idx]);

            swss::FieldValueTuple fvt(field, value);

            values.push_back(fvt);
        }

        countersTable.set(strPortId, values, "");
    }
}

std::vector<sai_port_stat_counter_t> getSupportedCounters(sai_object_id_t portId)
{
    SWSS_LOG_ENTER();

    std::vector<sai_port_stat_counter_t> supportedCounters;

    for (int idx = SAI_PORT_STAT_IF_IN_OCTETS;
         idx <= SAI_PORT_STAT_ETHER_OUT_PKTS_9217_TO_16383_OCTETS;
         ++idx)
    {
        sai_port_stat_counter_t counter = (sai_port_stat_counter_t)idx;

        uint64_t value;

        sai_status_t status = sai_port_api->get_port_stats(portId, &counter, 1, &value);

        if (status != SAI_STATUS_SUCCESS)
        {
            const std::string &name = sai_serialize_port_stat(counter);

            SWSS_LOG_WARN("counter %s is not supported on port RID 0x%lx: %d", name.c_str(), portId, status);
            continue;
        }

        supportedCounters.push_back(counter);
    }

    return std::move(supportedCounters);
}

static volatile bool  g_runCountersThread = false;
static std::shared_ptr<std::thread> g_countersThread = NULL;

static std::mutex mtx_sleep;
static std::condition_variable cv_sleep;

void collectCountersThread(int intervalInSeconds)
{
    SWSS_LOG_ENTER();

    swss::DBConnector db(COUNTERS_DB, swss::DBConnector::DEFAULT_UNIXSOCKET, 0);
    swss::Table countersTable(&db, "COUNTERS");

    auto ports = saiGetPortList();

    // get supported counters on first port
    // we assume that all ports will support those counters
    const auto &supportedCounters = getSupportedCounters(ports.at(0));

    SWSS_LOG_INFO("supported counters count: %ld", supportedCounters.size());

    while(g_runCountersThread)
    {
        collectCounters(countersTable, supportedCounters);

        std::unique_lock<std::mutex> lk(mtx_sleep);
        cv_sleep.wait_for(lk, std::chrono::seconds(intervalInSeconds));
    }
}

void startCountersThread(int intervalInSeconds)
{
    SWSS_LOG_ENTER();

    g_runCountersThread = true;

    g_countersThread = std::shared_ptr<std::thread>(new std::thread(collectCountersThread, intervalInSeconds));
}

void endCountersThread()
{
    SWSS_LOG_ENTER();

    g_runCountersThread = false;

    cv_sleep.notify_all();

    if (g_countersThread != NULL)
    {
        SWSS_LOG_INFO("counters thread join");

        g_countersThread->join();
    }

    SWSS_LOG_INFO("counters thread ended");
}
