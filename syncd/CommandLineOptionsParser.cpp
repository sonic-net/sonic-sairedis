#include "CommandLineOptionsParser.h"

#include <getopt.h>

std::shared_ptr<CommandLineOptions> CommandLineOptionsParser::parseCommandLine(
        _In_ int argc,
        _In_ char **argv)
{
    SWSS_LOG_ENTER();

    auto options = std::make_shared<CommandLineOptions>();

#ifdef SAITHRIFT
    const char* const optstring = "dp:t:uSUCsrm:h";
#else
    const char* const optstring = "dp:t:uSUCsh";
#endif // SAITHRIFT

    while(true)
    {
        static struct option long_options[] =
        {
            { "diag",                    no_argument,       0, 'd' },
            { "profile",                 required_argument, 0, 'p' },
            { "startType",               required_argument, 0, 't' },
            { "useTempView",             no_argument,       0, 'u' },
            { "disableExitSleep",        no_argument,       0, 'S' },
            { "enableUnittests",         no_argument,       0, 'U' },
            { "enableConsistencyCheck",  no_argument,       0, 'C' },
            { "syncMode",                no_argument,       0, 's' },
#ifdef SAITHRIFT
            { "rpcserver",               no_argument,       0, 'r' },
            { "portmap",                 required_argument, 0, 'm' },
#endif // SAITHRIFT
            { "help",                    no_argument,       0, 'h' },
            { 0,                         0,                 0,  0  }
        };

        int option_index = 0;

        int c = getopt_long(argc, argv, optstring, long_options, &option_index);

        if (c == -1)
        {
            break;
        }

        switch (c)
        {
            case 'd':
                options->m_enableDiagShell = true;
                break;

            case 'p':
                options->m_profileMapFile = std::string(optarg);
                break;

            case 't':
                options->m_startType = CommandLineOptions::startTypeStringToStartType(optarg);

                if (options->m_startType == SAI_START_TYPE_UNKNOWN)
                {
                    SWSS_LOG_ERROR("unknown start type '%s'", optarg);
                    exit(EXIT_FAILURE);
                }
                break;

            case 'u':
                options->m_enableTempView = true;
                break;

            case 'S':
                options->m_disableExitSleep = true;
                break;

            case 'U':
                options->m_enableUnittests = true;
                break;

            case 'C':
                options->m_enableConsistencyCheck = true;
                break;

            case 's':
                options->m_enableSyncMode = true;
                break;

#ifdef SAITHRIFT
            case 'r':
                options->m_runRPCServer = true;
                break;
            case 'm':
                options->m_portMapFile = std::string(optarg);
                break;
#endif // SAITHRIFT

            case 'h':
                printUsage();
                exit(EXIT_SUCCESS);

            case '?':
                SWSS_LOG_WARN("unknown option %c", optopt);
                printUsage();
                exit(EXIT_FAILURE);

            default:
                SWSS_LOG_ERROR("getopt_long failure");
                exit(EXIT_FAILURE);
        }
    }

    return options;
}

void CommandLineOptionsParser::printUsage()
{
    SWSS_LOG_ENTER();

#ifdef SAITHRIFT
    std::cout << "Usage: syncd [-d] [-p profile] [-t type] [-u] [-S] [-U] [-C] [-s] [-r] [-m portmap] [-h]" << std::endl;
#else
    std::cout << "Usage: syncd [-d] [-p profile] [-t type] [-u] [-S] [-U] [-C] [-s] [-h]" << std::endl;
#endif // SAITHRIFT

    std::cout << "    -d --diag" << std::endl;
    std::cout << "        Enable diagnostic shell" << std::endl;
    std::cout << "    -p --profile profile" << std::endl;
    std::cout << "        Provide profile map file" << std::endl;
    std::cout << "    -t --startType type" << std::endl;
    std::cout << "        Specify start type (cold|warm|fast|fastfast) " << std::endl;
    std::cout << "    -u --useTempView" << std::endl;
    std::cout << "        Use temporary view between init and apply" << std::endl;
    std::cout << "    -S --disableExitSleep" << std::endl;
    std::cout << "        Disable sleep when syncd crashes" << std::endl;
    std::cout << "    -U --enableUnittests" << std::endl;
    std::cout << "        Metadata enable unittests" << std::endl;
    std::cout << "    -C --enableConsistencyCheck" << std::endl;
    std::cout << "        Enable consisteny check DB vs ASIC after comparison logic" << std::endl;
    std::cout << "    -s --syncMode" << std::endl;
    std::cout << "        Enable synchronous mode" << std::endl;

#ifdef SAITHRIFT

    std::cout << "    -r --rpcserver" << std::endl;
    std::cout << "        Enable rpcserver" << std::endl;
    std::cout << "    -m --portmap portmap" << std::endl;
    std::cout << "        Specify port map file" << std::endl;

#endif // SAITHRIFT

    std::cout << "    -h --help" << std::endl;
    std::cout << "        Print out this message" << std::endl;
}

