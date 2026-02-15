#include <iostream>
#include <sstream>
#include <ctime>
#include <sstream>

#include <unistd.h>
#include <getopt.h>

#include "swss/logger.h"

extern "C" {
#include <sai.h>
}

// TODO split to multiple cpp

std::string sai_profile = "/tmp/sai.profile";

void print_usage()
{
    SWSS_LOG_ENTER();

    std::cerr << "Following SAI dump options can be specified:" << std::endl;
    std::cerr << "-------------------------------------------" << std::endl;
    std::cerr << "--dump_file -f   Full path for dump file" << std::endl;
    std::cerr << "--profile -p     Full path to SAI profile file [ default is " << sai_profile << " ]" << std::endl;
    std::cerr << "--help  -h       usage" << std::endl;
}

__attribute__((__noreturn__)) void exit_with_sai_failure(const char *msg, sai_status_t status)
{
    SWSS_LOG_ENTER();

    if (msg)
    {
        std::cerr << msg << " rc=" << status << std::endl;
    }

    SWSS_LOG_ERROR("saisdkdump exited with SAI rc: 0x%x, msg: %s .", status, (msg != NULL ? msg : ""));
    exit(EXIT_FAILURE);
}

const char* profile_get_value(
        _In_ sai_switch_profile_id_t profile_id,
        _In_ const char* variable)
{
    SWSS_LOG_ENTER();

    return sai_profile.c_str();
}

int profile_get_next_value(
        _In_ sai_switch_profile_id_t profile_id,
        _Out_ const char** variable,
        _Out_ const char** value)
{
    SWSS_LOG_ENTER();
    return -1;
}

sai_service_method_table_t test_services = {
    profile_get_value,
    profile_get_next_value
};

int main(int argc, char **argv)
{
    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_NOTICE);

    SWSS_LOG_ENTER();

    static struct option longOptions[] =
    {
        { "help", no_argument, 0, 'h' },
        { "dump_file", required_argument, 0, 'f' },
        { "profile", required_argument, 0, 'p' }
    };

    bool fileSpecified = false;
    std::string fileName;
    int option_index = 0;
    int c = 0;

    while ((c = getopt_long(argc, argv, "hf:p:", longOptions, &option_index)) != -1)
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
            case 'p':
                if (optarg != NULL)
                {
                    sai_profile = std::string(optarg);
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
        struct tm local_tm;
        struct tm *now = localtime_r(&t, &local_tm);
        strStream << "/tmp/saisdkdump_" << now->tm_mday << "_" << now->tm_mon + 1 << "_" << now->tm_year + 1900 << "_" << now->tm_hour << "_" << now->tm_min << "_" << now->tm_sec;
        fileName = strStream.str();
        SWSS_LOG_INFO("The dump file is not specified, generated \"%s\" file name", fileName.c_str());
    }

    sai_status_t status = sai_api_initialize(0, (sai_service_method_table_t*)&test_services);
    if (status != SAI_STATUS_SUCCESS)
    {
    	exit_with_sai_failure("Failed to initialize SAI api", status);
    }

    sai_switch_api_t* switch_api;
    status = sai_api_query(SAI_API_SWITCH, (void**) &switch_api);
    if (status != SAI_STATUS_SUCCESS)
    {
        exit_with_sai_failure("Failed to query switch api", status);
    }

    sai_object_id_t switch_id;
    const uint32_t AttributesCount = 2;
    sai_attribute_t attrs[AttributesCount];
    attrs[0].id = SAI_SWITCH_ATTR_INIT_SWITCH;
    attrs[0].value.booldata = false;
    attrs[1].id = SAI_SWITCH_ATTR_UNINIT_DATA_PLANE_ON_REMOVAL;
    attrs[1].value.booldata = false;

    status = switch_api->create_switch(&switch_id, AttributesCount, attrs);
    if (status != SAI_STATUS_SUCCESS)
    {
        exit_with_sai_failure("Failed to create a switch", status);
    }

    status = sai_dbg_generate_dump(fileName.c_str());
    if (status != SAI_STATUS_SUCCESS)
    {
        exit_with_sai_failure("Failed to generate SAI dump", status);
    }

    SWSS_LOG_NOTICE("The SAI dump is generated to %s .", fileName.c_str());
    std::cout << "The SAI dump is generated to " << fileName << std::endl;

    status = switch_api->remove_switch(switch_id);
    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("remove switch 0x%lx failed: 0x%x", switch_id, status);
    }

    status = sai_api_uninitialize();
    if (status != SAI_STATUS_SUCCESS)
    {
        exit_with_sai_failure("SAI api uninitialize failed", status);
    }

    return EXIT_SUCCESS;
}
