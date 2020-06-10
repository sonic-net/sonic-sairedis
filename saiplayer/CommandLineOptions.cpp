#include "CommandLineOptions.h"

#include "swss/logger.h"

#include <sstream>

using namespace saiplayer;

CommandLineOptions::CommandLineOptions()
{
    SWSS_LOG_ENTER();

    // default values for command line options

    m_useTempView = false;
    m_inspectAsic = false;
    m_skipNotifySyncd = false;
    m_enableDebug = false;
    m_sleep = false;
    m_syncMode = false;

    m_profileMapFile = "";
    m_contextConfig = "";
}

std::string CommandLineOptions::getCommandLineString() const
{
    SWSS_LOG_ENTER();

    std::stringstream ss;

    ss << " UseTempView=" << (m_useTempView ? "YES" : "NO");
    ss << " InspectAsic=" << (m_inspectAsic ? "YES" : "NO");
    ss << " SkipNotifySyncd=" << (m_skipNotifySyncd ? "YES" : "NO");
    ss << " EnableDebug=" << (m_enableDebug ? "YES" : "NO");
    ss << " Sleep=" << (m_sleep ? "YES" : "NO");
    ss << " SyncMode=" << (m_syncMode ? "YES" : "NO");
    ss << " ProfileMapFile=" << m_profileMapFile;
    ss << " ContextConfig=" << m_contextConfig;

    return ss.str();
}
