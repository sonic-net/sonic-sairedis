#pragma once

#include "swss/sal.h"
#include "swss/table.h"

#include <thread>
#include <atomic>
#include <functional>
#include <mutex>

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

            void setEventData(
                    _In_ const std::string& eventName,
                    _In_ const swss::KeyOpFieldsValuesTuple* kco = nullptr);

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

            void logEventData();

        private:

            volatile bool m_run;

            // all values are in microseconds

            std::atomic_int_fast64_t m_warnTimespan;
            std::atomic_int_fast64_t m_startTimestamp;
            std::atomic_int_fast64_t m_endTimestamp;
            std::atomic_int_fast64_t m_lastCheckTimestamp;

            std::shared_ptr<std::thread> m_thread;

            std::function<void(uint64_t)> m_callback;

            std::string m_eventName;

            const swss::KeyOpFieldsValuesTuple* m_kco;

            std::mutex m_mutex;
    };
}
