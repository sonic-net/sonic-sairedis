#include "saidump.h"

using namespace syncd;

int main(int argc, char **argv)
{
    SWSS_LOG_ENTER();
    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_DEBUG);
    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_NOTICE);
    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_INFO);

    SaiDump m_saiDump;

    if(SAI_STATUS_SUCCESS != m_saiDump.handleCmdLine(argc, argv))
    {
        return EXIT_FAILURE;
    }

    m_saiDump.dumpFromRedisDb();
    return EXIT_SUCCESS;
}