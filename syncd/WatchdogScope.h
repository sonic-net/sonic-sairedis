#pragma once

#include "TimerWatchdog.h"

namespace syncd
{
    class WatchdogScope
    {
        public:

            WatchdogScope(
                    _In_ TimerWatchdog& tw,
                    _In_ const std::string eventName);

            ~WatchdogScope();

        private:

            TimerWatchdog& m_tw;
    };
}
