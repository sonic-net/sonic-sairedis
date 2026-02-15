#pragma once

#include "TrafficFilter.h"

#include <memory>
#include <map>
#include <mutex>

namespace saivs
{
    class TrafficFilterPipes
    {
        public:

            TrafficFilterPipes() = default;

            virtual ~TrafficFilterPipes() = default;

        public:

            bool installFilter(
                    _In_ int priority,
                    _In_ std::shared_ptr<TrafficFilter> filter);

            bool uninstallFilter(
                    _In_ std::shared_ptr<TrafficFilter> filter);

            TrafficFilter::FilterStatus execute(
                    _Inout_ void *buffer,
                    _Inout_ size_t &length);

        private:

            typedef std::map<int, std::shared_ptr<TrafficFilter> > FilterPriorityQueue;

            std::mutex m_mutex;

            FilterPriorityQueue m_filters;
    };
}
