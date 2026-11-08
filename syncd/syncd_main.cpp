#include "sairediscommon.h"

#include "CommandLineOptionsParser.h"
#include "Syncd.h"
#include "MetadataLogger.h"
#include "PortMapParser.h"

#include "swss/warm_restart.h"

#ifdef SAITHRIFT
#include <utility>
#include <algorithm>
#include <switch_sai_rpc_server.h>
#endif // SAITHRIFT

#ifdef SAITHRIFT
#define SWITCH_SAI_THRIFT_RPC_SERVER_PORT 9092
#endif // SAITHRIFT

using namespace syncd;

/*
 * Make sure that notification queue pointer is populated before we start
 * thread, and before we create_switch, since at switch_create we can start
 * receiving fdb_notifications which will arrive on different thread and
 * will call getQueueSize() when queue pointer could be null (this=0x0).
 */

int syncd_main(int argc, char **argv)
{
    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_DEBUG);

    SWSS_LOG_ENTER();

    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_NOTICE);

    swss::Logger::linkToDbNative("syncd");

    swss::WarmStart::initialize("syncd", "syncd");

    swss::WarmStart::checkWarmStart("syncd", "syncd");

    bool isWarmStart = swss::WarmStart::isWarmStart();

    MetadataLogger::initialize();

    auto commandLineOptions = CommandLineOptionsParser::parseCommandLine(argc, argv);

#ifdef SAITHRIFT

    if (commandLineOptions->m_portMapFile.size() > 0)
    {
        auto map = PortMapParser::parsePortMap(commandLineOptions->m_portMapFile);

        PortMap::setGlobalPortMap(map);
    }

    if (commandLineOptions->m_runRPCServer)
    {
        start_sai_thrift_rpc_server(SWITCH_SAI_THRIFT_RPC_SERVER_PORT);

        SWSS_LOG_NOTICE("rpcserver started");
    }

#endif // SAITHRIFT

    auto vendorSai = std::make_shared<VendorSai>();

    auto syncd = std::make_shared<Syncd>(vendorSai, commandLineOptions, isWarmStart);

    syncd->run();

    return EXIT_SUCCESS;
}
