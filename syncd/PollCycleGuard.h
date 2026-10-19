#pragma once

#include "swss/sal.h"

#include <condition_variable>

namespace syncd
{
    class PollCycleGuard
    {
        public:

            PollCycleGuard(
                    _Inout_ bool& inProgress,
                    _Inout_ std::condition_variable& condition);

            ~PollCycleGuard();

        private:

            bool& m_inProgress;

            std::condition_variable& m_condition;
    };
}
