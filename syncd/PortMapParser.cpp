#include "PortMapParser.h"

#include "swss/logger.h"

#include <nlohmann/json.hpp>

#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>

using namespace syncd;

static bool hasJsonExtension(const std::string& filename)
{
    SWSS_LOG_ENTER();

    const std::string ext = ".json";

    if (filename.size() < ext.size())
        return false;

    return filename.compare(filename.size() - ext.size(), ext.size(), ext) == 0;
}

// TODO: introduce common config format for SONiC
std::shared_ptr<PortMap> PortMapParser::parsePortMap(
        _In_ const std::string& portMapFile)
{
    SWSS_LOG_ENTER();

    auto portMap = std::make_shared<PortMap>();

    if (portMapFile.size() == 0)
    {
        SWSS_LOG_NOTICE("no port map file, returning empty port map");

        return portMap;
    }

    if (hasJsonExtension(portMapFile))
    {
        return parsePortMapFromJson(portMapFile);
    }

    std::ifstream portmap(portMapFile);

    if (!portmap.is_open())
    {
        std::cerr << "failed to open port map file:" << portMapFile.c_str() << " : "<< strerror(errno) << std::endl;
        SWSS_LOG_ERROR("failed to open port map file: %s: errno: %s", portMapFile.c_str(), strerror(errno));
        exit(EXIT_FAILURE);
    }

    std::string line;

    while (getline(portmap, line))
    {
        if (line.size() > 0 && (line[0] == '#' || line[0] == ';'))
        {
            continue;
        }

        std::istringstream iss(line);
        std::string name, lanes, alias;
        iss >> name >> lanes >> alias;

        iss.clear();
        iss.str(lanes);
        std::string lane_str;
        std::set<int> lane_set;

        while (getline(iss, lane_str, ','))
        {
            int lane = stoi(lane_str);
            lane_set.insert(lane);
        }

        portMap->insert(lane_set, name);
    }

    SWSS_LOG_NOTICE("returning port map with %zu entries", portMap->size());

    return portMap;
}

std::shared_ptr<PortMap> PortMapParser::parsePortMapFromJson(
        _In_ const std::string& jsonFile)
{
    SWSS_LOG_ENTER();

    auto portMap = std::make_shared<PortMap>();

    std::ifstream ifs(jsonFile);

    if (!ifs.is_open())
    {
        std::cerr << "failed to open JSON port map file:" << jsonFile.c_str() << " : " << strerror(errno) << std::endl;
        SWSS_LOG_ERROR("failed to open JSON port map file: %s: errno: %s", jsonFile.c_str(), strerror(errno));
        exit(EXIT_FAILURE);
    }

    nlohmann::json j;

    try
    {
        ifs >> j;
    }
    catch (const nlohmann::json::parse_error& e)
    {
        SWSS_LOG_ERROR("failed to parse JSON file %s: %s", jsonFile.c_str(), e.what());
        exit(EXIT_FAILURE);
    }

    if (!j.contains("PORT") || !j["PORT"].is_object())
    {
        SWSS_LOG_ERROR("JSON file %s does not contain a valid PORT table", jsonFile.c_str());
        exit(EXIT_FAILURE);
    }

    for (const auto& item : j["PORT"].items())
    {
        const auto& portName = item.key();
        const auto& portData = item.value();

        if (!portData.contains("lanes") || !portData["lanes"].is_string())
        {
            SWSS_LOG_WARN("skipping port %s: missing or invalid lanes field", portName.c_str());
            continue;
        }

        std::string lanes = portData["lanes"].get<std::string>();

        std::istringstream iss(lanes);
        std::string lane_str;
        std::set<int> lane_set;

        while (getline(iss, lane_str, ','))
        {
            try
            {
                lane_set.insert(stoi(lane_str));
            }
            catch (const std::exception& e)
            {
                SWSS_LOG_ERROR("invalid lane value '%s' for port %s: %s", lane_str.c_str(), portName.c_str(), e.what());
                exit(EXIT_FAILURE);
            }
        }

        portMap->insert(lane_set, portName);
    }

    SWSS_LOG_NOTICE("returning port map from JSON with %zu entries", portMap->size());

    return portMap;
}
