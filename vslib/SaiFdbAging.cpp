#include "Sai.h"
#include "SaiInternal.h"

#include "swss/logger.h"
#include "swss/select.h"

using namespace saivs;

/**
 * @brief FDB aging thread timeout in milliseconds.
 *
 * Every timeout aging FDB will be performed.
 */
#define FDB_AGING_THREAD_TIMEOUT_MS (1000)

void Sai::startFdbAgingThread()
{
    SWSS_LOG_ENTER();

    m_fdbAgingThreadEvent = std::make_shared<swss::SelectableEvent>();

    m_fdbAgingThreadRun = true;

    // XXX should this be moved to create switch and SwitchState?

    m_fdbAgingThread = std::make_shared<std::thread>(&Sai::fdbAgingThreadProc, this);
}

void Sai::stopFdbAgingThread()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("begin");

    if (m_fdbAgingThreadRun)
    {
        m_fdbAgingThreadRun = false;

        m_fdbAgingThreadEvent->notify();

        m_fdbAgingThread->join();
    }

    SWSS_LOG_NOTICE("end");
}

void Sai::processFdbEntriesForAging()
{
    MUTEX();
    SWSS_LOG_ENTER();

    // must be executed under mutex since
    // this call comes from other thread

    m_vsSai->ageFdbs();
}

void Sai::fdbAgingThreadProc()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("begin");

    swss::Select s;

    s.addSelectable(m_fdbAgingThreadEvent.get());

    while (m_fdbAgingThreadRun)
    {
        swss::Selectable *sel = nullptr;

        int result = s.select(&sel, FDB_AGING_THREAD_TIMEOUT_MS);

        if (sel == m_fdbAgingThreadEvent.get())
        {
            // user requested shutdown_switch
            break;
        }

        if (result == swss::Select::TIMEOUT)
        {
            processFdbEntriesForAging();
        }
    }

    SWSS_LOG_NOTICE("end");
}
