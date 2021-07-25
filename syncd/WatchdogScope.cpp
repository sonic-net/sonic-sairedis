#include "WatchdogScope.h"

using namespace syncd;

WatchdogScope::WatchdogScope(
        _In_ TimerWatchdog& tw,
        _In_ const std::string eventName):
    m_tw(tw)
{
    // SWSS_LOG_ENTER(); // disabled

    m_tw.setStartTime();

    m_tw.setEventName(eventName);
}

WatchdogScope::~WatchdogScope()
{
    // SWSS_LOG_ENTER(); // disabled

    m_tw.setEndTime();
}
