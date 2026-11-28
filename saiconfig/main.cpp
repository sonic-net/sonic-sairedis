#include "SaiConfig.h"
#include "swss/logger.h"

#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <vector>

using namespace std;
using namespace syncd;
using namespace sairedis;

int main(int argc, char **argv)
{
    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_ERROR);

    SWSS_LOG_ENTER();

    // Set default log level to ERROR to suppress verbose NOTICE logs
    // Users can still see errors and use --verbose flag if needed

    try
    {
        SaiConfig saiConfig;
        saiConfig.handleCmdLine(argc, argv);
        return EXIT_SUCCESS;
    }
    catch (const exception &e)
    {
        SWSS_LOG_ERROR("Exception in main: %s", e.what());
        cerr << "Error: " << e.what() << endl;
        return EXIT_FAILURE;
    }
}
