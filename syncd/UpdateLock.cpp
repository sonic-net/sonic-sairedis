#include "UpdateLock.h"

#include "swss/logger.h"

using namespace syncd;

UpdateLock::UpdateLock(
        _Inout_ std::mutex& mutex,
        _Inout_ std::atomic<uint32_t>& pending,
        _Inout_ std::condition_variable& condition):
    m_pending(pending),
    m_condition(condition),
    m_lock(mutex, std::defer_lock)
{
    SWSS_LOG_ENTER();

    ++m_pending;

    try
    {
        m_lock.lock();
    }
    catch (...)
    {
        --m_pending;
        m_condition.notify_all();

        throw;
    }
}

UpdateLock::~UpdateLock()
{
    SWSS_LOG_ENTER();

    if (m_lock.owns_lock())
    {
        unlock();
    }
}

void UpdateLock::unlock()
{
    SWSS_LOG_ENTER();

    --m_pending;
    m_condition.notify_all();
    m_lock.unlock();
}

void UpdateLock::waitForPollEnd(
        _In_ const bool& pollInProgress,
        _Inout_ uint32_t& deferred,
        _In_ const std::atomic<bool>& running)
{
    SWSS_LOG_ENTER();

    if (!pollInProgress || !running)
    {
        return;
    }

    // Support probes can themselves read and clear an existing object's
    // counters. Let its original sweep and plugins finish before probing.
    // Withdraw from the checkpoint drain while waiting; the separate count
    // prevents a new sweep from overtaking these updates.
    ++deferred;
    --m_pending;
    m_condition.notify_all();

    try
    {
        m_condition.wait(m_lock, [&]
        {
            return !pollInProgress || !running;
        });
    }
    catch (...)
    {
        --deferred;
        ++m_pending;
        m_condition.notify_all();

        throw;
    }

    --deferred;
    ++m_pending;
    m_condition.notify_all();
}
