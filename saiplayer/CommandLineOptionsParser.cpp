#include "CommandLineOptionsParser.h"

#include "swss/logger.h"

#include <getopt.h>

#include <iostream>

using namespace saiplayer;

std::shared_ptr<CommandLineOptions> CommandLineOptionsParser::parseCommandLine(
        _In_ int argc,
        _In_ char **argv)
{
    SWSS_LOG_ENTER();

    auto options = std::make_shared<CommandLineOptions>();

    const char* const optstring = "uiCdsmp:x:h";

    while(true)
    {
        static struct option long_options[] =
        {
            { "useTempView",      no_argument,       0, 'u' },
            { "inspectAsic",      no_argument,       0, 'i' },
            { "skipNotifySyncd",  no_argument,       0, 'C' },
            { "enableDebug",      no_argument,       0, 'd' },
            { "sleep",            no_argument,       0, 's' },
            { "syncMode",         no_argument,       0, 'm' },
            { "profile",          required_argument, 0, 'p' },
            { "contextContig",    required_argument, 0, 'x' },
            { "help",             no_argument,       0, 'h' },
        };

        int option_index = 0;

        int c = getopt_long(argc, argv, optstring, long_options, &option_index);

        if (c == -1)
        {
            break;
        }

        switch (c)
        {
            case 'u':
                options->m_useTempView = true;
                break;

            case 'i':
                options->m_inspectAsic = true;
                break;

            case 'C':
                options->m_skipNotifySyncd = true;
                break;

            case 'd':
                options->m_enableDebug = true;
                break;

            case 's':
                options->m_sleep = true;
                break;

            case 'm':
                options->m_syncMode = true;
                break;

            case 'x':
                options->m_contextConfig = std::string(optarg);
                break;

            case 'h':
                printUsage();
                exit(EXIT_SUCCESS);

            case 'p':
                options->m_profileMapFile = std::string(optarg);
                break;

            case '?':
                SWSS_LOG_WARN("unknown option %c", optopt);
                printUsage();
                exit(EXIT_FAILURE);

            default:
                SWSS_LOG_ERROR("getopt_long failure");
                exit(EXIT_FAILURE);
        }
    }


    for (int i = optind; i < argc; i++)
    {
        options->m_files.push_back(argv[i]);
    }

    if (options->m_files.size() == 0)
    {
        SWSS_LOG_ERROR("no files to replay");
        exit(EXIT_FAILURE);
    }

    return options;
}

void CommandLineOptionsParser::printUsage()
{
    SWSS_LOG_ENTER();

    std::cout << "Usage: saiplayer [-u] [-i] [-C] [-d] [-s] [-m] [-p profile] [-x contextConfig] [-h] recordfile" << std::endl << std::endl;

    std::cout << "    -u --useTempView:" << std::endl;
    std::cout << "        Enable temporary view between init and apply" << std::endl << std::endl;
    std::cout << "    -i --inspectAsic:" << std::endl;
    std::cout << "        Inspect ASIC by ASIC DB" << std::endl << std::endl;
    std::cout << "    -C --skipNotifySyncd:" << std::endl;
    std::cout << "        Will not send notify init/apply view to syncd" << std::endl << std::endl;
    std::cout << "    -d --enableDebug:" << std::endl;
    std::cout << "        Enable syslog debug messages" << std::endl << std::endl;
    std::cout << "    -s --sleep:" << std::endl;
    std::cout << "        Sleep after success reply, to notice any switch notifications" << std::endl << std::endl;
    std::cout << "    -m --syncMode:" << std::endl;
    std::cout << "        Enable synchronous mode" << std::endl << std::endl;
    std::cout << "    -p --profile profile" << std::endl;
    std::cout << "        Provide profile map file" << std::endl;
    std::cout << "    -x --contextConfig" << std::endl;
    std::cout << "        Context configuration file" << std::endl;

    std::cout << "    -h --help:" << std::endl;
    std::cout << "        Print out this message" << std::endl << std::endl;
}
