#pragma once

#include "FlexCounterSyncState.h"

#include "swss/sal.h"

namespace syncd
{
    class PollCycleGuard
    {
        public:

            PollCycleGuard(
                    _Inout_ FlexCounterSyncState& state);

            ~PollCycleGuard();

        private:

            FlexCounterSyncState& m_state;
    };
}
