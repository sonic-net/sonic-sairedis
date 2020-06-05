#pragma once

#include <thread>
#include <atomic>

#ifndef _In_
#define _In_ 
#endif

class TimerWatchdog
{
    typedef void (*Callback)(
            _In_ int64_t span);

    public:

    TimerWatchdog(
            _In_ int64_t warnTimespan);

    virtual ~TimerWatchdog();

    public:

    void setStartTime();

    void setEndTime();

    void setCallback(
            _In_ Callback callback);

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

    Callback m_callback;

};
