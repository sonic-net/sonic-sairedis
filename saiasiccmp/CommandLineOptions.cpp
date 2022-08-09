#include "CommandLineOptions.h"

#include "meta/sai_serialize.h"

#include "swss/logger.h"

#include <sstream>

using namespace saiasiccmp;

CommandLineOptions::CommandLineOptions()
{
    SWSS_LOG_ENTER();

    // default values for command line options

    m_enableLogLevelInfo = false;
    m_dumpDiffToStdErr = false;
}

std::string CommandLineOptions::getCommandLineString() const
{
    SWSS_LOG_ENTER();

    std::stringstream ss;

    ss << " EnableLogLevelInfo=" << (m_enableLogLevelInfo ? "YES" : "NO");
    ss << " DumpDiffToStdErr=" << (m_dumpDiffToStdErr ? "YES" : "NO");

    for (auto &arg: m_args)
    {
        ss << " " << arg ;
    }

    return ss.str();
}
