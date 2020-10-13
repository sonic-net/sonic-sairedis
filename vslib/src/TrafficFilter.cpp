#include "TrafficFilter.h"

using namespace saivs;

bool TrafficFilterPipes::installFilter(
    int priority,
    std::shared_ptr<TrafficFilter> filter)
{
    std::unique_lock<std::mutex> guard(m_mutex);
    return m_filters.emplace(priority, filter).second;
}

bool TrafficFilterPipes::uninstallFilter(std::shared_ptr<TrafficFilter> filter)
{
    std::unique_lock<std::mutex> guard(m_mutex);
    for (auto itr = m_filters.begin();
        itr != m_filters.end();
        itr ++)
    {
        if (itr->second == filter)
        {
            m_filters.erase(itr);
            return true;
        }
    }
    return false;
}

TrafficFilter::FilterStatus TrafficFilterPipes::execute(
    void *buffer,
    ssize_t &length)
{
    std::unique_lock<std::mutex> guard(m_mutex);
    TrafficFilter::FilterStatus ret = TrafficFilter::CONTINUE;
    for (auto itr = m_filters.begin(); itr != m_filters.end();)
    {
        auto filter = itr->second;
        if (filter)
        {
            ret = filter->execute(
                buffer,
                length);
            if (ret == TrafficFilter::CONTINUE)
            {
                itr ++;
            }
            else
            {
                break;
            }
        }
        else
        {
            itr = m_filters.erase(itr);
        }
        
    }
    return ret;
}
