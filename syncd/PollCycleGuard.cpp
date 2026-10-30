#include "PollCycleGuard.h"

#include "swss/logger.h"

using namespace syncd;

PollCycleGuard::PollCycleGuard(
        _Inout_ FlexCounterSyncState& state):
    m_state(state)
{
    SWSS_LOG_ENTER();

    m_state.pollInProgress = true;
}

PollCycleGuard::~PollCycleGuard()
{
    SWSS_LOG_ENTER();

    m_state.pollInProgress = false;
    m_state.condition.notify_all();
}
