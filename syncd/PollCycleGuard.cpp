#include "PollCycleGuard.h"

#include "swss/logger.h"

using namespace syncd;

PollCycleGuard::PollCycleGuard(
        _Inout_ bool& inProgress,
        _Inout_ std::condition_variable& condition):
    m_inProgress(inProgress),
    m_condition(condition)
{
    SWSS_LOG_ENTER();

    m_inProgress = true;
}

PollCycleGuard::~PollCycleGuard()
{
    SWSS_LOG_ENTER();

    m_inProgress = false;
    m_condition.notify_all();
}
