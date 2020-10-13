#pragma once

#include <memory>
#include <map>
#include <mutex>

namespace saivs
{

enum FilterPriority
{
    MACSEC_FILTER,
};

class TrafficFilter
{
 public:
    enum FilterStatus
    {
        CONTINUE,
        TERMINATE,
        ERROR,
    };
    virtual FilterStatus execute(
        void *buffer,
        ssize_t &length) = 0;
};

// TODO : To use RCU strategy to update filter pipes
class TrafficFilterPipes
{
 public:

    TrafficFilterPipes() = default;
    ~TrafficFilterPipes() = default;

    bool installFilter(
        int priority,
        std::shared_ptr<TrafficFilter> filter);

    bool uninstallFilter(std::shared_ptr<TrafficFilter> filter);

    TrafficFilter::FilterStatus execute(void *buffer, ssize_t &length);

 private:
    typedef std::map<int, std::shared_ptr<TrafficFilter> > FilterPriorityQueue;
    std::mutex m_mutex;
    FilterPriorityQueue m_filters;
};

}  // namespace saivs
