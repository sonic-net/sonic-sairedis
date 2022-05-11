#include "EventThrottle.h"

#include "swss/logger.h"

using namespace syncd;

/*
 * Defines fdb event notification limit. When this many notification will be
 * seen in specific period of time then receiving of this event will be dropped.
 */
#define FDB_EVENT_NOTIFICATION_LIMIT (10)

#define FDB_EVENT_NOTIFICATION_LIMIT_TIMESPAN_NS (1000*1000*1000)
#define ONE_SECOND_TIMESPAN_NS (1000*1000*1000)

EventThrottle::EventThrottle(
        _In_ const std::string& name):
    m_name(name),
    m_count(0),
    m_dropped(0)
{
    SWSS_LOG_ENTER();

    m_time = getTime();
}

uint64_t EventThrottle::getTime()
{
    // SWSS_LOG_ENTER(); // disabled

    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

bool EventThrottle::shouldEnqueue()
{
    // SWSS_LOG_ENTER(); // disabled

    auto time = getTime();

    if (time < m_time)
    {
        m_dropped++;
        return false;
    }

    if (m_dropped)
    {
        SWSS_LOG_WARN("dropped fdb event %s %ld times", m_name.c_str(), m_dropped);
        m_dropped = 0;
    }

    if (m_count == 0)
    {
        m_time = getTime(); // first notification time stamp
    }

    m_count++;

    if (m_count < FDB_EVENT_NOTIFICATION_LIMIT)
        return true;

    if (time - m_time < FDB_EVENT_NOTIFICATION_LIMIT_TIMESPAN_NS)
    {
        SWSS_LOG_WARN("fdb entry %s seen %ld times, dropping for 1 seconds",
                m_name.c_str(),
                m_count);

        // set time in the future for 1 second, this will cause drop
        m_time = time + ONE_SECOND_TIMESPAN_NS;
    }

    m_count = 0;

    return true;
}
