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
        { "warm", no_argument, 0, 'w' }
    };

    bool warmRestartHint = false;
    bool optionSpecified = false;

    while(true)
    {
        int option_index = 0;

        int c = getopt_long(argc, argv, "cw", long_options, &option_index);

        if (c == -1)
            break;

        switch (c)
        {
            case 'c':
                warmRestartHint = false;
                optionSpecified = true;
                break;

            case 'w':
                warmRestartHint = true;
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
        std::cerr << " --cold  -c   for cold restart" << std::endl;

        exit(EXIT_FAILURE);
    }

    swss::DBConnector db(ASIC_DB, swss::DBConnector::DEFAULT_UNIXSOCKET, 0);
    swss::NotificationProducer restartQuery(&db, "RESTARTQUERY");

    std::vector<swss::FieldValueTuple> values;

    std::string op = warmRestartHint ? "WARM" : "COLD";

    SWSS_LOG_NOTICE("requested %s shutdown", op.c_str());

    std::cerr << "requested " << op << " shutdown" << std::endl;

    restartQuery.send(op, op, values);

    return EXIT_SUCCESS;
}
