#include "LaneMapFileParser.h"

#include "swss/logger.h"
#include "swss/tokenize.h"

#include <net/if.h>

#include <fstream>

using namespace saivs;

// must be the same or less as SAI_VS_SWITCH_INDEX_MAX
#define MAX_SWITCH_INDEX (255)

bool LaneMapFileParser::isInterfaceNameValid(
        _In_ const std::string& name)
{
    SWSS_LOG_ENTER();

    size_t size = name.size();

    if (size == 0 || size > IFNAMSIZ)
    {
        SWSS_LOG_ERROR("invalid interface name %s or length: %zu", name.c_str(), size);
        return false;
    }

    for (size_t i = 0; i < size; i++)
    {
        char c = name[i];

        if (c >= '0' && c <= '9')
            continue;

        if (c >= 'a' && c <= 'z')
            continue;

        if (c >= 'A' && c <= 'Z')
            continue;

        SWSS_LOG_ERROR("invalid character '%c' in interface name %s", c, name.c_str());
        return false;
    }

    // interface name is valid
    return true;
}

void LaneMapFileParser::parse(
        _In_ std::shared_ptr<LaneMapContainer> container,
        _In_ uint32_t switchIndex,
        _In_ const std::string& ifname,
        _In_ const std::string& slanes)
{
    SWSS_LOG_ENTER();

    if (!isInterfaceNameValid(ifname))
    {
        SWSS_LOG_ERROR("interface name '%s' is invalid", ifname.c_str());
        return;
    }

    auto tokens = swss::tokenize(slanes,',');

    size_t n = tokens.size();

    if (n != 1 && n != 2 && n != 4)
    {
        SWSS_LOG_ERROR("invalid number of lanes (%zu) assigned to interface %s", n, ifname.c_str());
        return;
    }

    std::vector<uint32_t> lanes;

    for (auto l: tokens)
    {
        uint32_t lanenumber;
        if (sscanf(l.c_str(), "%u", &lanenumber) != 1)
        {
            SWSS_LOG_ERROR("failed to parse lane number: %s", l.c_str());
            continue;
        }

        lanes.push_back(lanenumber);
    }

    auto lm = container->getLaneMap(switchIndex);

    if (!lm)
    {
        lm = std::make_shared<LaneMap>(switchIndex);

        container->insert(lm);
    }

    lm->add(ifname, lanes);
}

void LaneMapFileParser::parseLineWithNoIndex(
        _In_ std::shared_ptr<LaneMapContainer> container,
        _In_ const std::vector<std::string>& tokens)
{
    SWSS_LOG_ENTER();

    auto ifname = tokens.at(0);
    auto slanes = tokens.at(1);

    parse(container, LaneMap::DEFAULT_SWITCH_INDEX, ifname, slanes);
}

void LaneMapFileParser::parseLineWithIndex(
        _In_ std::shared_ptr<LaneMapContainer> container,
        _In_ const std::vector<std::string>& tokens)
{
    SWSS_LOG_ENTER();

    auto swidx  = tokens.at(0);
    auto ifname = tokens.at(1);
    auto slanes = tokens.at(2);

    uint32_t switchIndex;

    if (sscanf(swidx.c_str(), "%u", & switchIndex) != 1)
    {
        SWSS_LOG_ERROR("failed to parse switchIndex: %s", swidx.c_str());
        return;
    }

    parse(container, switchIndex, ifname, slanes);
}

std::shared_ptr<LaneMapContainer> LaneMapFileParser::parseLaneMapFile(
        _In_ const std::string& file)
{
    SWSS_LOG_ENTER();

    auto container = std::make_shared<LaneMapContainer>();

    std::ifstream ifs(file);

    if (!ifs.is_open())
    {
        SWSS_LOG_WARN("failed to open lane map file: %s, using default", file.c_str());

        auto def = LaneMap::getDefaultLaneMap();

        container->insert(def);

        return container;
    }

    SWSS_LOG_NOTICE("loading lane map from: %s", file.c_str());

    std::string line;

    while (getline(ifs, line))
    {
        /*
         * line can be in 2 forms:
         * ethX:lane,...
         * N:ethX:lane,...
         *
         * where N is switchIndex (0..255) - SAI_VS_SWITCH_INDEX_MAX
         * if N is not specified then zero (0) is assumed
         */

        if (line.size() > 0 && (line[0] == '#' || line[0] == ';'))
        {
            continue;
        }

        SWSS_LOG_INFO("lane line: %s", line.c_str());

        auto tokens = swss::tokenize(line, ':');

        if (tokens.size() == 3)
        {
            parseLineWithIndex(container, tokens);
        }
        else if (tokens.size() == 2)
        {
            parseLineWithNoIndex(container, tokens);
        }
        else
        {
            SWSS_LOG_ERROR("expected 2 or 3 tokens in line %s, got %zu", line.c_str(), tokens.size());
        }
    }

    container->removeEmptyLaneMaps();

    if (container->size() == 0)
    {
        SWSS_LOG_WARN("no lane map loaded, returning default lane map");

        auto def = LaneMap::getDefaultLaneMap();

        container->insert(def);

        return container;
    }

    SWSS_LOG_NOTICE("loaded %zu lane maps to container", container->size());

    return container;
}

std::shared_ptr<LaneMapContainer> LaneMapFileParser::parseLaneMapFile(
        _In_ const char* file)
{
    SWSS_LOG_ENTER();

    if (file == nullptr)
    {
        auto container = std::make_shared<LaneMapContainer>();

        SWSS_LOG_NOTICE("no file map file specified, loading default");

        auto def = LaneMap::getDefaultLaneMap();

        container->insert(def);

        return container;
    }

    std::string name(file);

    return parseLaneMapFile(name);
}

