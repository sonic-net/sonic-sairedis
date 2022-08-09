#pragma once

#include "TimerWatchdog.h"

namespace syncd
{
    class WatchdogScope
    {
        public:

            /**
             * @brief Watchdog scope constructor.
             *
             * @param timerWatchdog - Timer watchdog to work on.
             * @param eventName - Name of the event.
             * @param data - Event data. NOTE: event data is passed here as a
             * pointer here to avoid expensive data copy, but then we must be
             * sure that that data object was not destroyed before watchdog
             * scope destructor will be called. This will make sure that if
             * event was too long, then it data will be logged into syslog from
             * object that is still alive and not destroyed.
             */
            WatchdogScope(
                    _In_ TimerWatchdog& timerWatchdog,
                    _In_ const std::string& eventName,
                    _In_ const swss::KeyOpFieldsValuesTuple* data = nullptr);

            ~WatchdogScope();

        private:

            TimerWatchdog& m_tw;
    };
}
