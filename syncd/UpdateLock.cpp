#include "UpdateLock.h"

#include "swss/logger.h"

using namespace syncd;

UpdateLock::UpdateLock(
        _Inout_ FlexCounterSyncState& state):
    m_state(state),
    m_lock(state.mutex, std::defer_lock)
{
    SWSS_LOG_ENTER();

    ++m_state.pendingUpdates;

    try
    {
        m_lock.lock();
    }
    catch (...)
    {
        --m_state.pendingUpdates;
        m_state.condition.notify_all();

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

    --m_state.pendingUpdates;
    m_state.condition.notify_all();
    m_lock.unlock();
}

void UpdateLock::waitForPollEnd(
        _In_ const std::atomic<bool>& running)
{
    SWSS_LOG_ENTER();

    if (!m_state.pollInProgress || !running)
    {
        return;
    }

    // Support probes can themselves read and clear an existing object's
    // counters. Let its original sweep and plugins finish before probing.
    // Withdraw from the checkpoint drain while waiting; the separate count
    // prevents a new sweep from overtaking these updates.
    ++m_state.deferredUpdates;
    --m_state.pendingUpdates;
    m_state.condition.notify_all();

    try
    {
        m_state.condition.wait(m_lock, [&]
        {
            return !m_state.pollInProgress || !running;
        });
    }
    catch (...)
    {
        --m_state.deferredUpdates;
        ++m_state.pendingUpdates;
        m_state.condition.notify_all();

        throw;
    }

    --m_state.deferredUpdates;
    ++m_state.pendingUpdates;
    m_state.condition.notify_all();
}
