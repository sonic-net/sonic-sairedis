#include "PerformanceIntervalTimer.h"

#include "swss/logger.h"

using namespace sairediscommon;

bool PerformanceIntervalTimer::m_enable = true;

PerformanceIntervalTimer::PerformanceIntervalTimer(
        _In_ const char*msg,
        _In_ uint64_t limit):
    m_msg(msg),
    m_limit(limit)
{
    SWSS_LOG_ENTER();

    reset();
}

void PerformanceIntervalTimer::start()
{
    SWSS_LOG_ENTER();

    m_start = std::chrono::high_resolution_clock::now();
}

void PerformanceIntervalTimer::stop()
{
    SWSS_LOG_ENTER();

    m_stop = std::chrono::high_resolution_clock::now();
}

void PerformanceIntervalTimer::inc(
        _In_ uint64_t val)
{
    SWSS_LOG_ENTER();

    m_count += val;

    m_calls++;

    m_total += std::chrono::duration_cast<std::chrono::nanoseconds>(m_stop-m_start).count();

    if (m_count >= m_limit)
    {
        if (m_enable)
        {
            SWSS_LOG_NOTICE("%zu (calls %zu) %s op took: %zu ms", m_count, m_calls, m_msg.c_str(), m_total/1000000);
        }

        reset();
    }
}

void PerformanceIntervalTimer::reset()
{
    SWSS_LOG_ENTER();

    m_count = 0;
    m_calls = 0;
    m_total = 0;
}
