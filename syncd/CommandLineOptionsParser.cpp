#include "CommandLineOptionsParser.h"

#include "meta/sai_serialize.h"

#include "swss/logger.h"

#include <getopt.h>

#include <iostream>

using namespace syncd;

std::shared_ptr<CommandLineOptions> CommandLineOptionsParser::parseCommandLine(
        _In_ int argc,
        _In_ char **argv)
{
    SWSS_LOG_ENTER();

    auto options = std::make_shared<CommandLineOptions>();

#ifdef SAITHRIFT
    const char* const optstring = "dp:t:g:x:b:B:w:uSUCsz:lrm:h";
#else
    const char* const optstring = "dp:t:g:x:b:B:w:uSUCsz:lh";
#endif // SAITHRIFT

    while (true)
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
            { "redisCommunicationMode",  required_argument, 0, 'z' },
            { "enableSaiBulkSupport",    no_argument,       0, 'l' },
            { "globalContext",           required_argument, 0, 'g' },
            { "contextContig",           required_argument, 0, 'x' },
            { "breakConfig",             required_argument, 0, 'b' },
            { "watchdogWarnTimeSpan",    optional_argument, 0, 'w' },
            { "supportingBulkCounters",  required_argument, 0, 'B' },
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
                SWSS_LOG_WARN("param -s is depreacated, use -z");
                options->m_enableSyncMode = true;
                break;

            case 'z':
                sai_deserialize_redis_communication_mode(optarg, options->m_redisCommunicationMode);
                break;

            case 'l':
                options->m_enableSaiBulkSupport = true;
                break;

            case 'g':
                options->m_globalContext = (uint32_t)std::stoul(optarg);
                break;

            case 'x':
                options->m_contextConfig = std::string(optarg);
                break;

            case 'b':
                options->m_breakConfig = std::string(optarg);
                break;

            case 'w':
                options->m_watchdogWarnTimeSpan = (int64_t)std::stoll(optarg);
                break;

#ifdef SAITHRIFT
            case 'r':
                options->m_runRPCServer = true;
                break;
            case 'm':
                options->m_portMapFile = std::string(optarg);
                break;
#endif // SAITHRIFT

            case 'B':
                options->m_supportingBulkCounterGroups = std::string(optarg);
                break;

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
    std::cout << "Usage: syncd [-d] [-p profile] [-t type] [-u] [-S] [-U] [-C] [-s] [-z mode] [-l] [-g idx] [-x contextConfig] [-b breakConfig] [-B supportingBulkCounters] [-r] [-m portmap] [-h]" << std::endl;
#else
    std::cout << "Usage: syncd [-d] [-p profile] [-t type] [-u] [-S] [-U] [-C] [-s] [-z mode] [-l] [-g idx] [-x contextConfig] [-b breakConfig] [-B supportingBulkCounters] [-h]" << std::endl;
#endif // SAITHRIFT

    std::cout << "    -d --diag" << std::endl;
    std::cout << "        Enable diagnostic shell" << std::endl;
    std::cout << "    -p --profile profile" << std::endl;
    std::cout << "        Provide profile map file" << std::endl;
    std::cout << "    -t --startType type" << std::endl;
    std::cout << "        Specify start type (cold|warm|fast|fastfast|express)" << std::endl;
    std::cout << "    -u --useTempView" << std::endl;
    std::cout << "        Use temporary view between init and apply" << std::endl;
    std::cout << "    -S --disableExitSleep" << std::endl;
    std::cout << "        Disable sleep when syncd crashes" << std::endl;
    std::cout << "    -U --enableUnittests" << std::endl;
    std::cout << "        Metadata enable unittests" << std::endl;
    std::cout << "    -C --enableConsistencyCheck" << std::endl;
    std::cout << "        Enable consisteny check DB vs ASIC after comparison logic" << std::endl;
    std::cout << "    -s --syncMode" << std::endl;
    std::cout << "        Enable synchronous mode (depreacated, use -z)" << std::endl;
    std::cout << "    -z --redisCommunicationMode" << std::endl;
    std::cout << "        Redis communication mode (redis_async|redis_sync|zmq_sync), default: redis_async" << std::endl;
    std::cout << "    -l --enableBulk" << std::endl;
    std::cout << "        Enable SAI Bulk support" << std::endl;
    std::cout << "    -g --globalContext" << std::endl;
    std::cout << "        Global context index to load from context config file" << std::endl;
    std::cout << "    -x --contextConfig" << std::endl;
    std::cout << "        Context configuration file" << std::endl;
    std::cout << "    -b --breakConfig" << std::endl;
    std::cout << "        Comparison logic 'break before make' configuration file" << std::endl;
    std::cout << "    -w --watchdogWarnTimeSpan" << std::endl;
    std::cout << "        Watchdog time span (in microseconds) to watch for execution" << std::endl;
    std::cout << "    -B --supportingBulkCounters" << std::endl;
    std::cout << "        Counter groups those support bulk polling" << std::endl;

#ifdef SAITHRIFT

    std::cout << "    -r --rpcserver" << std::endl;
    std::cout << "        Enable rpcserver" << std::endl;
    std::cout << "    -m --portmap portmap" << std::endl;
    std::cout << "        Specify port map file" << std::endl;

#endif // SAITHRIFT

    std::cout << "    -h --help" << std::endl;
    std::cout << "        Print out this message" << std::endl;
}
