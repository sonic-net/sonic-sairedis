#include "SaiPlayer.h"
#include "CommandLineOptionsParser.h"

#include "lib/inc/ClientServerSai.h"
#include "syncd/MetadataLogger.h"

#include "swss/logger.h"

using namespace saiplayer;

int main(int argc, char **argv)
{
    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_DEBUG);

    SWSS_LOG_ENTER();

    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_NOTICE);

    syncd::MetadataLogger::initialize();

    auto commandLineOptions = CommandLineOptionsParser::parseCommandLine(argc, argv);

    auto sai = std::make_shared<sairedis::ClientServerSai>();

    auto player = std::make_shared<SaiPlayer>(sai, commandLineOptions);

    return player->run();
}
