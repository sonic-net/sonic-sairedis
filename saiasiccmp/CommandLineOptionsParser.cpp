#include "CommandLineOptionsParser.h"

#include "meta/sai_serialize.h"

#include "swss/logger.h"

#include <getopt.h>

#include <iostream>

using namespace saiasiccmp;

std::shared_ptr<CommandLineOptions> CommandLineOptionsParser::parseCommandLine(
        _In_ int argc,
        _In_ char **argv)
{
    SWSS_LOG_ENTER();

    auto options = std::make_shared<CommandLineOptions>();

    const char* const optstring = "idh";

    while (true)
    {
        static struct option long_options[] =
        {
            { "enableLogLevelInfo",      no_argument,       0, 'i' },
            { "dumpDiffToStdErr",        no_argument,       0, 'd' },
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
            case 'i':
                options->m_enableLogLevelInfo = true;
                break;

            case 'd':
                options->m_dumpDiffToStdErr = true;
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

    for (int index = optind; index < argc; index++)
    {
        options->m_args.push_back(argv[index]);
    }

    return options;
}

void CommandLineOptionsParser::printUsage()
{
    SWSS_LOG_ENTER();

    std::cout << "Usage: saiasiccmp [-i] [-d] [-h] file1 file2" << std::endl << std::endl;

    std::cout << "    file1 and file2 must be in json fromat produced by redis-dump-load" << std::endl;
    std::cout << "    for example: redisdl.py -d 1 -y" << std::endl << std::endl;

    std::cout << "    -i --enableLogLevelInfo" << std::endl;
    std::cout << "        Enable LogLevel INFO" << std::endl;
    std::cout << "    -d --dumpDiffToStdErr" << std::endl;
    std::cout << "        Dump asic diff to stderr" << std::endl;
    std::cout << "    -h --help" << std::endl;
    std::cout << "        Print out this message" << std::endl;
}
