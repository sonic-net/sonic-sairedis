#include "RequestShutdownCommandLineOptionsParser.h"

#include "swss/logger.h"

#include <unistd.h>
#include <getopt.h>

#include <ostream>
#include <iostream>

using namespace syncd;

std::shared_ptr<RequestShutdownCommandLineOptions> RequestShutdownCommandLineOptionsParser::parseCommandLine(
        _In_ int argc,
        _In_ char **argv)
{
    SWSS_LOG_ENTER();

    auto options = std::make_shared<RequestShutdownCommandLineOptions>();

    static struct option long_options[] =
    {
        { "cold", no_argument, 0, 'c' },
        { "warm", no_argument, 0, 'w' },
        { "fast", no_argument, 0, 'f' },
        { "pre",  no_argument, 0, 'p' }, // Requesting pre shutdown
        { "help", no_argument, 0, 'h' },

        { "globalContext", required_argument, 0, 'g' },
        { "contextContig", required_argument, 0, 'x' },
        { 0,               0,                 0,  0  }
    };

    bool optionSpecified = false;

    while (true)
    {
        int option_index = 0;

        int c = getopt_long(argc, argv, "cwfpg:x:h", long_options, &option_index);

        if (c == -1)
            break;

        switch (c)
        {
            case 'c':
                options->setRestartType(SYNCD_RESTART_TYPE_COLD);
                optionSpecified = true;
                break;

            case 'w':
                options->setRestartType(SYNCD_RESTART_TYPE_WARM);
                optionSpecified = true;
                break;

            case 'f':
                options->setRestartType(SYNCD_RESTART_TYPE_FAST);
                optionSpecified = true;
                break;

            case 'p':
                options->setRestartType(SYNCD_RESTART_TYPE_PRE_SHUTDOWN);
                optionSpecified = true;
                break;

            case 'g':
                options->m_globalContext = (uint32_t)std::stoul(optarg);
                break;

            case 'x':
                options->m_contextConfig = std::string(optarg);
                break
                    ;
            case 'h':
                printUsage();
                exit(EXIT_FAILURE);

            default:
                SWSS_LOG_ERROR("getopt failure");
                exit(EXIT_FAILURE);
        }
    }

    if (!optionSpecified)
    {
        SWSS_LOG_ERROR("no shutdown option specified");

        printUsage();

        exit(EXIT_FAILURE);
    }

    return options;
}

void RequestShutdownCommandLineOptionsParser::printUsage()
{
    SWSS_LOG_ENTER();

    std::cout << "Usage: syncd_request_shutdown [-w] [--warm] [-p] [--pre] [-c] [--cold] [-f] [--fast] [-g idx] [-x contextConfig] [-h] [--help]" << std::endl;

    std::cerr << std::endl;

    std::cerr << "Shutdown option must be specified" << std::endl;
    std::cerr << "---------------------------------" << std::endl;
    std::cerr << "  -w --warm" << std::endl;
    std::cerr << "      For warm restart" << std::endl;
    std::cerr << "  -p --pre" << std::endl;
    std::cerr << "      For warm pre-shutdown" << std::endl;
    std::cerr << "  -c --cold" << std::endl;
    std::cerr << "      For cold restart" << std::endl;
    std::cerr << "  -f --fast" << std::endl;
    std::cerr << "      For fast restart" << std::endl;
    std::cout << "  -g --globalContext " << std::endl;
    std::cout << "      Global context index to load from context config file" << std::endl;
    std::cout << "  -x --contextConfig" << std::endl;
    std::cout << "      Context configuration file" << std::endl;
    std::cout << "  -h --help" << std::endl;
    std::cout << "      Print usege" << std::endl;
}
