#pragma once

#include "swss/sal.h"

#include <chrono>
#include <string>

namespace sairediscommon
{
    class PerformanceIntervalTimer
    {
        public:

            static constexpr unsigned int DEFAULT_LIMIT = 10000;

        public:

            PerformanceIntervalTimer(
                    _In_ const char* msg,
                    _In_ uint64_t limit = DEFAULT_LIMIT);

            ~PerformanceIntervalTimer() = default; // non virtual

        public:

            void start();

            void stop();

            void inc(
                    _In_ uint64_t val = 1);

            void reset();

        public:

            static bool m_enable;

        private:

            std::string m_msg;

            std::chrono::time_point<std::chrono::high_resolution_clock> m_start;
            std::chrono::time_point<std::chrono::high_resolution_clock> m_stop;

            uint64_t m_limit;
            uint64_t m_count;
            uint64_t m_calls;
            uint64_t m_total;
    };
}
