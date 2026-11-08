#include "RequestShutdownCommandLineOptions.h"

#include "swss/logger.h"

using namespace syncd;

RequestShutdownCommandLineOptions::RequestShutdownCommandLineOptions():
    m_restartType(SYNCD_RESTART_TYPE_COLD)
{
    SWSS_LOG_ENTER();

    m_globalContext = 0;
}

RequestShutdownCommandLineOptions::~RequestShutdownCommandLineOptions()
{
    SWSS_LOG_ENTER();

    // empty
}

syncd_restart_type_t RequestShutdownCommandLineOptions::getRestartType() const
{
    SWSS_LOG_ENTER();

    return m_restartType;
}

void RequestShutdownCommandLineOptions::setRestartType(
        _In_ syncd_restart_type_t restartType)
{
    SWSS_LOG_ENTER();

    m_restartType = restartType;
}

#define STRING_RESTART_COLD         "COLD"
#define STRING_RESTART_WARM         "WARM"
#define STRING_RESTART_FAST         "FAST"
#define STRING_RESTART_EXPRESS      "EXPRESS"
#define STRING_RESTART_PRE_SHUTDOWN "PRE-SHUTDOWN"
#define STRING_RESTART_PRE_EXPRESS_SHUTDOWN "PRE-EXPRESS-SHUTDOWN"

syncd_restart_type_t RequestShutdownCommandLineOptions::stringToRestartType(
        _In_ const std::string& restartType)
{
    SWSS_LOG_ENTER();

    if (restartType == STRING_RESTART_COLD)
        return SYNCD_RESTART_TYPE_COLD;

    if (restartType == STRING_RESTART_WARM)
        return SYNCD_RESTART_TYPE_WARM;

    if (restartType == STRING_RESTART_FAST)
        return SYNCD_RESTART_TYPE_FAST;

    if (restartType == STRING_RESTART_EXPRESS)
        return SYNCD_RESTART_TYPE_EXPRESS;

    if (restartType == STRING_RESTART_PRE_SHUTDOWN)
        return SYNCD_RESTART_TYPE_PRE_SHUTDOWN;

    if (restartType == STRING_RESTART_PRE_EXPRESS_SHUTDOWN)
        return SYNCD_RESTART_TYPE_PRE_EXPRESS_SHUTDOWN;

    SWSS_LOG_WARN("unknown restart type '%s' returning COLD", restartType.c_str());

    return SYNCD_RESTART_TYPE_COLD;
}

std::string RequestShutdownCommandLineOptions::restartTypeToString(
        _In_ syncd_restart_type_t restartType)
{
    SWSS_LOG_ENTER();

    switch (restartType)
    {
        case SYNCD_RESTART_TYPE_COLD:
            return STRING_RESTART_COLD;

        case SYNCD_RESTART_TYPE_WARM:
            return STRING_RESTART_WARM;

        case SYNCD_RESTART_TYPE_FAST:
            return STRING_RESTART_FAST;

        case SYNCD_RESTART_TYPE_EXPRESS:
            return STRING_RESTART_EXPRESS;

        case SYNCD_RESTART_TYPE_PRE_SHUTDOWN:
            return STRING_RESTART_PRE_SHUTDOWN;

        case SYNCD_RESTART_TYPE_PRE_EXPRESS_SHUTDOWN:
            return STRING_RESTART_PRE_EXPRESS_SHUTDOWN;

        default:

            SWSS_LOG_WARN("unknown restart type '%d' returning COLD", restartType);
            return STRING_RESTART_COLD;
    }
}
