#include "CommandLineOptionsParser.h"
#include "AsicCmp.h"

#include "swss/logger.h"

#include <iostream>

using namespace saiasiccmp;

int main(int argc, char **argv)
{
    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_DEBUG);

    SWSS_LOG_ENTER();

    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_NOTICE);

    auto commandLineOptions = CommandLineOptionsParser::parseCommandLine(argc, argv);

    SWSS_LOG_NOTICE("command line: %s",  commandLineOptions->getCommandLineString().c_str());

    if (commandLineOptions->m_enableLogLevelInfo)
    {
        swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_INFO);
    }

    auto& args = commandLineOptions->m_args;

    if (args.size() != 2)
    {
        std::cerr << "ERROR: expected 2 input files, but given: " << args.size() << std::endl;
        exit(1);
    }

    AsicCmp asicCmp(commandLineOptions);

    bool equal = asicCmp.compare();

    std::cerr << "views are" << (equal ? " " : " NOT " ) << "equal" << std::endl;

    return equal ? EXIT_SUCCESS : EXIT_FAILURE;
}
