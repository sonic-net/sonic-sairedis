#include <iostream>
#include <sstream>

#include <unistd.h>
#include <getopt.h>

#include "swss/notificationproducer.h"
#include "swss/logger.h"

int main(int argc, char **argv)
{
    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_NOTICE);

    SWSS_LOG_ENTER();

    static struct option long_options[] =
    {
        { "cold", no_argument, 0, 'c' },
        { "warm", no_argument, 0, 'w' },
        { "fast", no_argument, 0, 'f' },
        { "pre",  no_argument, 0, 'p' }, // Requesting pre shutdown
    };

    std::string op;
    bool optionSpecified = false;

    while(true)
    {
        int option_index = 0;

        int c = getopt_long(argc, argv, "cwfp", long_options, &option_index);

        if (c == -1)
            break;

        switch (c)
        {
            case 'c':
                op = "COLD";
                optionSpecified = true;
                break;

            case 'w':
                op = "WARM";
                optionSpecified = true;
                break;

            case 'f':
                op = "FAST";
                optionSpecified = true;
                break;

            case 'p':
                op = "PRE-SHUTDOWN";
                optionSpecified = true;
                break;

            default:
                SWSS_LOG_ERROR("getopt failure");
                exit(EXIT_FAILURE);
        }
    }

    if (!optionSpecified)
    {
        SWSS_LOG_ERROR("no shutdown option specified");

        std::cerr << "Shutdown option must be specified" << std::endl;
        std::cerr << "---------------------------------" << std::endl;
        std::cerr << " --warm  -w   for warm restart" << std::endl;
        std::cerr << " --pre   -p   for warm pre-shutdown" << std::endl;
        std::cerr << " --cold  -c   for cold restart" << std::endl;
        std::cerr << " --fast  -f   for fast restart" << std::endl;

        exit(EXIT_FAILURE);
    }

    swss::DBConnector db(ASIC_DB, swss::DBConnector::DEFAULT_UNIXSOCKET, 0);
    swss::NotificationProducer restartQuery(&db, "RESTARTQUERY");

    std::vector<swss::FieldValueTuple> values;

    SWSS_LOG_NOTICE("requested %s shutdown", op.c_str());

    std::cerr << "requested " << op << " shutdown" << std::endl;

    restartQuery.send(op, op, values);

    return EXIT_SUCCESS;
}
