#include "CorePortIndexMapFileParser.h"

#include "swss/logger.h"
#include "swss/tokenize.h"

#include <net/if.h>

#include <fstream>
#include <cctype>

using namespace saivs;

// must be the same or less as SAI_VS_SWITCH_INDEX_MAX
#define MAX_SWITCH_INDEX (255)

bool CorePortIndexMapFileParser::isInterfaceNameValid(
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

        if (std::isalnum(c))
            continue;

        SWSS_LOG_ERROR("invalid character '%c' in interface name %s", c, name.c_str());
        return false;
    }

    // interface name is valid
    return true;
}

void CorePortIndexMapFileParser::parse(
        _In_ std::shared_ptr<CorePortIndexMapContainer> container,
        _In_ uint32_t switchIndex,
        _In_ const std::string& ifname,
        _In_ const std::string& scpidx)
{
    SWSS_LOG_ENTER();

    if (!isInterfaceNameValid(ifname))
    {
        SWSS_LOG_ERROR("interface name '%s' is invalid", ifname.c_str());
        return;
    }

    auto tokens = swss::tokenize(scpidx, ',');

    size_t n = tokens.size();

    if (n != 2)
    {
        SWSS_LOG_ERROR("Invalid core port index map %s assigned to interface %s", scpidx.c_str(), ifname.c_str());
        return;
    }

    std::vector<uint32_t> cpidx;

    for (auto c: tokens)
    {
        uint32_t c_or_pidx;

        if (sscanf(c.c_str(), "%u", &c_or_pidx) != 1)
        {
            SWSS_LOG_ERROR("failed to parse core or port index: %s", c.c_str());
            continue;
        }

        cpidx.push_back(c_or_pidx);
    }

    auto corePortIndexMap = container->getCorePortIndexMap(switchIndex);

    if (!corePortIndexMap)
    {
        corePortIndexMap = std::make_shared<CorePortIndexMap>(switchIndex);

        container->insert(corePortIndexMap);
    }

    corePortIndexMap->add(ifname, cpidx);
}

void CorePortIndexMapFileParser::parseLineWithNoIndex(
        _In_ std::shared_ptr<CorePortIndexMapContainer> container,
        _In_ const std::vector<std::string>& tokens)
{
    SWSS_LOG_ENTER();

    auto ifname = tokens.at(0);
    auto scpidx = tokens.at(1);

    parse(container, CorePortIndexMap::DEFAULT_SWITCH_INDEX, ifname, scpidx);
}

void CorePortIndexMapFileParser::parseLineWithIndex(
        _In_ std::shared_ptr<CorePortIndexMapContainer> container,
        _In_ const std::vector<std::string>& tokens)
{
    SWSS_LOG_ENTER();

    auto swidx  = tokens.at(0);
    auto ifname = tokens.at(1);
    auto scpidx = tokens.at(2);

    uint32_t switchIndex;

    if (sscanf(swidx.c_str(), "%u", & switchIndex) != 1)
    {
        SWSS_LOG_ERROR("failed to parse switchIndex: %s", swidx.c_str());
        return;
    }

    parse(container, switchIndex, ifname, scpidx);
}

std::shared_ptr<CorePortIndexMapContainer> CorePortIndexMapFileParser::parseCorePortIndexMapFile(
        _In_ const std::string& file)
{
    SWSS_LOG_ENTER();

    auto container = std::make_shared<CorePortIndexMapContainer>();

    std::ifstream ifs(file);

    if (!ifs.is_open())
    {
        SWSS_LOG_WARN("failed to open core port index map file: %s, using default", file.c_str());

        auto def = CorePortIndexMap::getDefaultCorePortIndexMap();

        container->insert(def);

        return container;
    }

    SWSS_LOG_NOTICE("loading core port index map from: %s", file.c_str());

    std::string line;

    while (getline(ifs, line))
    {
        /*
         * line can be in 2 forms:
         * ethX:core, core port index
         * N:ethX:core, core port index
         *
         * where N is switchIndex (0..255) - SAI_VS_SWITCH_INDEX_MAX
         * if N is not specified then zero (0) is assumed
         */

        if (line.size() > 0 && (line[0] == '#' || line[0] == ';'))
        {
            continue;
        }

        SWSS_LOG_INFO("core port index line: %s", line.c_str());

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

    container->removeEmptyCorePortIndexMaps();

    if (container->size() == 0)
    {
        SWSS_LOG_WARN("no core port index map loaded, returning default core port index map");

        auto def = CorePortIndexMap::getDefaultCorePortIndexMap();

        container->insert(def);

        return container;
    }

    SWSS_LOG_NOTICE("loaded %zu core port index maps to container", container->size());

    return container;
}

std::shared_ptr<CorePortIndexMapContainer> CorePortIndexMapFileParser::parseCorePortIndexMapFile(
        _In_ const char* file)
{
    SWSS_LOG_ENTER();

    if (file == nullptr)
    {
        auto container = std::make_shared<CorePortIndexMapContainer>();

        SWSS_LOG_NOTICE("no file map file specified, loading default");

        auto def = CorePortIndexMap::getDefaultCorePortIndexMap();

        container->insert(def);

        return container;
    }

    std::string name(file);

    return parseCorePortIndexMapFile(name);
}
