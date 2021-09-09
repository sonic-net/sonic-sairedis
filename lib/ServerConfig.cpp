#include "ServerConfig.h"

#include "swss/logger.h"
#include "swss/json.hpp"

#include <cstring>
#include <fstream>

using json = nlohmann::json;

using namespace sairedis;

ServerConfig::ServerConfig():
    m_zmqEndpoint("ipc:///tmp/saiServer"),
    m_zmqNtfEndpoint("ipc:///tmp/saiServerNtf")
{
    SWSS_LOG_ENTER();

    // empty intentionally
}

ServerConfig::~ServerConfig()
{
    SWSS_LOG_ENTER();

    // empty intentionally
}

std::shared_ptr<ServerConfig> ServerConfig::loadFromFile(
        _In_ const char* path)
{
    SWSS_LOG_ENTER();

    if (path == nullptr || strlen(path) == 0)
    {
        SWSS_LOG_NOTICE("no server config specified, will load default");

        return std::make_shared<ServerConfig>();
    }

    std::ifstream ifs(path);

    if (!ifs.good())
    {
        SWSS_LOG_ERROR("failed to read '%s', err: %s, returning default", path, strerror(errno));

        return std::make_shared<ServerConfig>();
    }

    try
    {
        json j;
        ifs >> j;

        auto cc = std::make_shared<ServerConfig>();

        cc->m_zmqEndpoint = j["zmq_endpoint"];
        cc->m_zmqNtfEndpoint = j["zmq_ntf_endpoint"];

        SWSS_LOG_NOTICE("server config: %s, %s",
                cc->m_zmqEndpoint.c_str(),
                cc->m_zmqNtfEndpoint.c_str());

        SWSS_LOG_NOTICE("loaded %s server config", path);

        return cc;
    }
    catch (const std::exception& e)
    {
        SWSS_LOG_ERROR("Failed to load '%s': %s, returning default", path, e.what());

        return std::make_shared<ServerConfig>();
    }
}
