#include "CommandLineOptions.h"

#include "meta/sai_serialize.h"

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
    m_enableRecording = false;

    m_redisCommunicationMode = SAI_REDIS_COMMUNICATION_MODE_REDIS_ASYNC;

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
    ss << " RedisCommunicationMode=" << sai_serialize_redis_communication_mode(m_redisCommunicationMode);
    ss << " EnableRecording=" << (m_enableRecording ? "YES" : "NO");
    ss << " ProfileMapFile=" << m_profileMapFile;
    ss << " ContextConfig=" << m_contextConfig;

    return ss.str();
}
