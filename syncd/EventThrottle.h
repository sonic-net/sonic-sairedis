#pragma once

#include "swss/sal.h"

#include <string>

namespace syncd
{
    class EventThrottle
    {
        public:

            EventThrottle(
                    _In_ const std::string& name);

        public:

            static uint64_t getTime();

            bool shouldEnqueue();

        private:

            std::string m_name;

            uint64_t m_time;
            uint64_t m_count;
            uint64_t m_dropped;
    };
}
