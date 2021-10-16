#include "WatchdogScope.h"

using namespace syncd;

WatchdogScope::WatchdogScope(
        _In_ TimerWatchdog& tw,
        _In_ const std::string& eventName,
        _In_ const swss::KeyOpFieldsValuesTuple* kco):
    m_tw(tw)
{
    // SWSS_LOG_ENTER(); // disabled

    m_tw.setStartTime();

    m_tw.setEventData(eventName, kco);
}

WatchdogScope::~WatchdogScope()
{
    // SWSS_LOG_ENTER(); // disabled

    m_tw.setEndTime();
}
