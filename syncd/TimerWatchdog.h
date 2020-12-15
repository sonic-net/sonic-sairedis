#pragma once

#include "swss/sal.h"

#include <thread>
#include <atomic>
#include <functional>

namespace syncd
{
    class TimerWatchdog
    {
        public:

            TimerWatchdog(
                    _In_ int64_t warnTimespan);

            virtual ~TimerWatchdog();

        public:

            void setStartTime();

            void setEndTime();

            void setCallback(
                    _In_ std::function<void(uint64_t)> callback);

            /**
             * @brief Gets timestamp since epoch.
             *
             * @return Time since epoch in microseconds.
             */
            static int64_t getTimeSinceEpoch();

        private:

            void threadFunction();

        private:

            volatile bool m_run;

            // all values are in microseconds

            std::atomic_int_fast64_t m_warnTimespan;
            std::atomic_int_fast64_t m_startTimestamp;
            std::atomic_int_fast64_t m_endTimestamp;
            std::atomic_int_fast64_t m_lastCheckTimestamp;

            std::shared_ptr<std::thread> m_thread;

            std::function<void(uint64_t)> m_callback;
    };
}
