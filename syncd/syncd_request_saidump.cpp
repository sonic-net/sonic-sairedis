#include <iostream>
#include <sstream>
#include <ctime>
#include <sstream>

#include <unistd.h>
#include <getopt.h>

#include "swss/redisclient.h"
#include "swss/dbconnector.h"
#include "swss/notificationconsumer.h"
#include "swss/notificationproducer.h"
#include "swss/selectableevent.h"
#include "swss/select.h"
#include "swss/logger.h"

void print_usage()
{
    std::cerr << "Following SAI dump options can be specified:" << std::endl;
    std::cerr << "-------------------------------------------" << std::endl;
    std::cerr << "--dump_file -f   Full path for dump file" << std::endl;
    std::cerr << "--help  -h       usage" << std::endl;
}

/* Parse the response and check if SAI dump was generated successfully */
bool handleDumpResponse(swss::NotificationConsumer &dumpResp)
{
    std::string respons;
    std::vector<swss::FieldValueTuple> values;
    dumpResp.pop(respons, respons, values);
    return (respons == "success");
}

int main(int argc, char **argv)
{
    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_NOTICE);

    SWSS_LOG_ENTER();

    static struct option longOptions[] =
    {
        { "help", no_argument, 0, 'h' },
        { "dump_file", required_argument, 0, 'f' }
    };

    bool fileSpecified = false;
    std::string fileName;
    int option_index = 0;
    int c = 0;

    while((c = getopt_long(argc, argv, "hf:", longOptions, &option_index)) != -1)
    {
        switch (c)
        {
            case 'f':
                if (optarg != NULL)
                {
            	    fileName = std::string(optarg);
                    fileSpecified = true;
                }
                break;

            case 'h':
                print_usage();
                break;

            default:
                SWSS_LOG_ERROR("getopt failure");
                exit(EXIT_FAILURE);
        }
    }

    if (!fileSpecified)
    {
    	std::ostringstream strStream;
    	time_t t = time(NULL);
    	struct tm *now = localtime(&t);
    	strStream << "/tmp/syncd_saidump_" << now->tm_mday << "_" << now->tm_mon + 1 << "_" << now->tm_year + 1900 << "_" << now->tm_hour << "_" << now->tm_min << "_" << now->tm_sec;
    	fileName = strStream.str();
    	SWSS_LOG_INFO("The dump file is not specified, generated \"%s\" file name", fileName.c_str());
    }

    /* Send the request to generate the SAI dump and wait for response */
    swss::DBConnector db(ASIC_DB, swss::DBConnector::DEFAULT_UNIXSOCKET, 0);
    swss::NotificationProducer dumpQuery(&db, "SAIDUMPQUERY");
    swss::NotificationConsumer dumpResp(&db, "SAIDUMPRESP");
    swss::Select s;
    s.addSelectable(&dumpResp);

    std::vector<swss::FieldValueTuple> values;
    dumpQuery.send(fileName, fileName, values);

    swss::Selectable *sel = NULL;
    int fd;
    int rc = s.select(&sel, &fd, 5000);
    if (rc != swss::Select::ERROR && rc != swss::Select::TIMEOUT && sel == &dumpResp && handleDumpResponse(dumpResp))
    {
        SWSS_LOG_NOTICE("The SAI dump is generated to %s", fileName.c_str());
        std::cout << "The SAI dump is generated to " << fileName  << std::endl;
    }
    else
    {
        SWSS_LOG_ERROR("Error! The SAI dump is not generated!");
        std::cerr << "Error! The SAI dump is not generated!" << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
