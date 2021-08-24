#include "MACsecFilterStateGuard.h"

#include <swss/logger.h>

using namespace saivs;

MACsecFilterStateGuard::MACsecFilterStateGuard(
    _Inout_ MACsecFilter::MACsecFilterState &guarded_state,
    _In_ MACsecFilter::MACsecFilterState target_state):
    m_guarded_state(guarded_state)
{
    SWSS_LOG_ENTER();

    m_old_state = m_guarded_state;
    m_guarded_state = target_state;
}

MACsecFilterStateGuard::~MACsecFilterStateGuard()
{
    SWSS_LOG_ENTER();

    m_guarded_state = m_old_state;
}
