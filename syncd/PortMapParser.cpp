#include "PortMapParser.h"

#include "swss/logger.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>

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
