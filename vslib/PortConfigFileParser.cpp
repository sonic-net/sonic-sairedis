#include "PortConfigFileParser.h"

#include "swss/logger.h"

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <exception>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>

using namespace saivs;

namespace
{
    bool parseLaneSet(
            _In_ const std::string& value,
            _Out_ std::set<uint32_t>& lanes)
    {
        SWSS_LOG_ENTER();

        std::istringstream lane_stream(value);
        std::string lane_token;
        size_t lane_count = 0;

        while (std::getline(lane_stream, lane_token, ','))
        {
            if (lane_token.empty())
            {
                return false;
            }

            size_t parsed = 0;
            unsigned long lane;
            try
            {
                lane = std::stoul(lane_token, &parsed, 10);
            }
            catch (const std::exception&)
            {
                return false;
            }

            if (parsed != lane_token.size() ||
                lane > std::numeric_limits<uint32_t>::max())
            {
                return false;
            }

            lane_count++;
            if (!lanes.insert(static_cast<uint32_t>(lane)).second)
            {
                return false;
            }
        }

        return lane_count != 0 && lane_count == lanes.size();
    }
}

std::shared_ptr<PortConfigMap> PortConfigFileParser::parse(
        _In_ const char* file)
{
    SWSS_LOG_ENTER();

    if (file == nullptr)
    {
        SWSS_LOG_WARN("no port config file specified");
        return std::make_shared<PortConfigMap>();
    }

    return parse(std::string(file));
}

std::shared_ptr<PortConfigMap> PortConfigFileParser::parse(
        _In_ const std::string& file)
{
    SWSS_LOG_ENTER();

    auto port_config = std::make_shared<PortConfigMap>();
    if (file.empty())
    {
        SWSS_LOG_WARN("no port config file specified");
        return port_config;
    }

    std::ifstream input(file);
    if (!input.is_open())
    {
        SWSS_LOG_WARN("failed to open port config file: %s: %s",
                file.c_str(), strerror(errno));
        return port_config;
    }

    std::string line;
    size_t line_number = 0;
    while (std::getline(input, line))
    {
        line_number++;
        std::istringstream fields(line);
        std::string name;
        std::string lane_value;

        if (!(fields >> name >> lane_value) || name[0] == '#' || name[0] == ';')
        {
            continue;
        }

        std::set<uint32_t> lanes;
        if (!parseLaneSet(lane_value, lanes))
        {
            SWSS_LOG_WARN("invalid port config lane list at %s:%zu",
                    file.c_str(), line_number);
            continue;
        }

        port_config->add(lanes, name);
    }

    SWSS_LOG_NOTICE("loaded %zu port config entries from %s",
            port_config->size(), file.c_str());
    return port_config;
}