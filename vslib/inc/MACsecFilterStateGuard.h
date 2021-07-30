#pragma once

#include "MACsecFilter.h"

namespace saivs
{
    class MACsecFilterStateGuard
    {
    public:
        MACsecFilterStateGuard(
            _Inout_ volatile MACsecFilter::MACsecFilterState &guarded_state,
            _In_ MACsecFilter::MACsecFilterState target_state);

        ~MACsecFilterStateGuard();

    private:
        MACsecFilter::MACsecFilterState m_old_state;
        volatile MACsecFilter::MACsecFilterState &m_guarded_state;
    };
}
