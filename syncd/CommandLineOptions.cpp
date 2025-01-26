#include "CommandLineOptions.h"

#include "meta/otai_serialize.h"

#include "swss/logger.h"

#include <sstream>

using namespace syncd;

CommandLineOptions::CommandLineOptions()
{
    SWSS_LOG_ENTER();

    // default values for command line options

    m_enableDiagShell = false;
    m_enableTempView = false;
    m_disableExitSleep = false;
    m_enableUnittests = false;
    m_enableConsistencyCheck = false;
    m_enableSyncMode = false;
    m_enableOtaiBulkSupport = false;

    m_redisCommunicationMode = OTAI_REDIS_COMMUNICATION_MODE_REDIS_ASYNC;

    m_startType = OTAI_START_TYPE_COLD_BOOT;

    m_profileMapFile = "";

    m_globalContext = 0;

    m_contextConfig = "";

    m_breakConfig = "";

    m_watchdogWarnTimeSpan = 30 * 1000000;

#ifdef OTAITHRIFT

    m_runRPCServer = false;

    m_portMapFile = "";

#endif // OTAITHRIFT

}

std::string CommandLineOptions::getCommandLineString() const
{
    SWSS_LOG_ENTER();

    std::stringstream ss;

    ss << " EnableDiagShell=" << (m_enableDiagShell ? "YES" : "NO");
    ss << " EnableTempView=" << (m_enableTempView ? "YES" : "NO");
    ss << " DisableExitSleep=" << (m_disableExitSleep ? "YES" : "NO");
    ss << " EnableUnittests=" << (m_enableUnittests ? "YES" : "NO");
    ss << " EnableConsistencyCheck=" << (m_enableConsistencyCheck ? "YES" : "NO");
    ss << " EnableSyncMode=" << (m_enableSyncMode ? "YES" : "NO");
    ss << " RedisCommunicationMode=" << otai_serialize_redis_communication_mode(m_redisCommunicationMode);
    ss << " EnableOtaiBulkSuport=" << (m_enableOtaiBulkSupport ? "YES" : "NO");
    ss << " StartType=" << startTypeToString(m_startType);
    ss << " ProfileMapFile=" << m_profileMapFile;
    ss << " GlobalContext=" << m_globalContext;
    ss << " ContextConfig=" << m_contextConfig;
    ss << " BreakConfig=" << m_breakConfig;
    ss << " WatchdogWarnTimeSpan=" << m_watchdogWarnTimeSpan;
    ss << " SupportingBulkCounters=" << m_supportingBulkCounterGroups;

#ifdef OTAITHRIFT

    ss << " RunRPCServer=" << (m_runRPCServer ? "YES" : "NO");
    ss << " PortMapFile=" << m_portMapFile;

#endif // OTAITHRIFT

    return ss.str();
}

otai_start_type_t CommandLineOptions::startTypeStringToStartType(
        _In_ const std::string& startType)
{
    SWSS_LOG_ENTER();

    if (startType == STRING_OTAI_START_TYPE_COLD_BOOT)
        return OTAI_START_TYPE_COLD_BOOT;

    if (startType == STRING_OTAI_START_TYPE_WARM_BOOT)
        return OTAI_START_TYPE_WARM_BOOT;

    if (startType == STRING_OTAI_START_TYPE_FAST_BOOT)
        return OTAI_START_TYPE_FAST_BOOT;

    if (startType == STRING_OTAI_START_TYPE_FASTFAST_BOOT)
        return OTAI_START_TYPE_FASTFAST_BOOT;

    if (startType == STRING_OTAI_START_TYPE_EXPRESS_BOOT)
        return OTAI_START_TYPE_EXPRESS_BOOT;

    if (startType == STRING_OTAI_START_TYPE_UNKNOWN)
        return OTAI_START_TYPE_UNKNOWN;

    SWSS_LOG_WARN("unknown startType: '%s'", startType.c_str());

    return OTAI_START_TYPE_UNKNOWN;
}

std::string CommandLineOptions::startTypeToString(
        _In_ otai_start_type_t startType)
{
    SWSS_LOG_ENTER();

    switch (startType)
    {
        case OTAI_START_TYPE_COLD_BOOT:
            return  STRING_OTAI_START_TYPE_COLD_BOOT;

        case OTAI_START_TYPE_WARM_BOOT:
            return STRING_OTAI_START_TYPE_WARM_BOOT;

        case OTAI_START_TYPE_FAST_BOOT:
            return STRING_OTAI_START_TYPE_FAST_BOOT;

        case OTAI_START_TYPE_FASTFAST_BOOT:
            return STRING_OTAI_START_TYPE_FASTFAST_BOOT;

        case OTAI_START_TYPE_EXPRESS_BOOT:
            return STRING_OTAI_START_TYPE_EXPRESS_BOOT;

        case OTAI_START_TYPE_UNKNOWN:
            return STRING_OTAI_START_TYPE_UNKNOWN;

        default:

            SWSS_LOG_WARN("unknown startType '%d'", startType);

            return STRING_OTAI_START_TYPE_UNKNOWN;
    }
}
