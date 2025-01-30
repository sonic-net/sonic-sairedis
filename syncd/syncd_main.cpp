#include "otairediscommon.h"

#include "CommandLineOptionsParser.h"
#include "Syncd.h"
#include "MetadataLogger.h"
#include "PortMapParser.h"

#include "swss/warm_restart.h"

#ifdef OTAITHRIFT
#include <utility>
#include <algorithm>
#include <switch_otai_rpc_server.h>
#endif // OTAITHRIFT

#ifdef OTAITHRIFT
#define SWITCH_OTAI_THRIFT_RPC_SERVER_PORT 9092
#endif // OTAITHRIFT

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

#ifdef OTAITHRIFT

    if (commandLineOptions->m_portMapFile.size() > 0)
    {
        auto map = PortMapParser::parsePortMap(commandLineOptions->m_portMapFile);

        PortMap::setGlobalPortMap(map);
    }

    if (commandLineOptions->m_runRPCServer)
    {
        start_otai_thrift_rpc_server(SWITCH_OTAI_THRIFT_RPC_SERVER_PORT);

        SWSS_LOG_NOTICE("rpcserver started");
    }

#endif // OTAITHRIFT

    auto vendorOtai = std::make_shared<VendorOtai>();

    auto syncd = std::make_shared<Syncd>(vendorOtai, commandLineOptions, isWarmStart);

    syncd->run();

    return EXIT_SUCCESS;
}
