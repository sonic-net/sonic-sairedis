#include "syncd.h"

#include <condition_variable>
#include <sstream>

static volatile bool  g_runCountersThread = false;
static std::shared_ptr<std::thread> g_countersThread = NULL;

static std::mutex mtx_sleep;
static std::condition_variable cv_sleep;

void collectCountersThread(
        _In_ int intervalInSeconds)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("starting counters thread");

    swss::DBConnector db(COUNTERS_DB, swss::DBConnector::DEFAULT_UNIXSOCKET, 0);
    swss::Table countersTable(&db, "COUNTERS");

    while (g_runCountersThread)
    {
        /*
         * NOTE: We know that at most we can have 1 switch
         * so we can use "COUNTERS" table, but for multiple
         * switches we need to do this per switch
         */

        for (auto sw: switches)
        {
            std::lock_guard<std::mutex> lock(g_mutex);

            /*
             * Collect counters should be under mutex since configuration can
             * change and we don't want that during counters collection.
             */

            sw.second->collectCounters(countersTable);
        }

        std::unique_lock<std::mutex> lk(mtx_sleep);

        cv_sleep.wait_for(lk, std::chrono::seconds(intervalInSeconds));
    }
}

void startCountersThread(
        _In_ int intervalInSeconds)
{
    SWSS_LOG_ENTER();

    if (g_runCountersThread)
    {
        SWSS_LOG_WARN("counter thread is already running");
        return;
    }

    g_runCountersThread = true;

    g_countersThread = std::make_shared<std::thread>(collectCountersThread, intervalInSeconds);
}

void endCountersThread()
{
    SWSS_LOG_ENTER();

    if (!g_runCountersThread)
    {
        SWSS_LOG_WARN("counter thread is not running");
        return;
    }

    g_runCountersThread = false;

    cv_sleep.notify_all();

    if (g_countersThread != NULL)
    {
        SWSS_LOG_INFO("counters thread join");

        g_countersThread->join();
    }

    SWSS_LOG_INFO("counters thread ended");
}
