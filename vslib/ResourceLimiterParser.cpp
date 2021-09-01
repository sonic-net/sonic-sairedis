#include "ResourceLimiterParser.h"

#include "swss/logger.h"
#include "swss/tokenize.h"

#include "meta/sai_serialize.h"

#include <fstream>

using namespace saivs;

std::shared_ptr<ResourceLimiterContainer> ResourceLimiterParser::parseFromFile(
        _In_ const char* fileName)
{
    SWSS_LOG_ENTER();

    if (fileName == nullptr)
    {
        SWSS_LOG_NOTICE("file name is NULL, returning empty limiter");

        return std::make_shared<ResourceLimiterContainer>();
    }

    std::string file(fileName);

    std::ifstream ifs(file);

    if (!ifs.is_open())
    {
        SWSS_LOG_WARN("failed to open resource limiter file: %s", file.c_str());

        return std::make_shared<ResourceLimiterContainer>();
    }

    SWSS_LOG_NOTICE("loading resource limits from: %s", file.c_str());

    std::string line;

    auto container = std::make_shared<ResourceLimiterContainer>();

    while (getline(ifs, line))
    {
        /*
         * line can be in 2 forms:
         *
         * SAI_OBJECT_TYPE_XXX=limit
         * N:SAI_OBJECT_TYPE_XXX=limit
         *
         * where N is switchIndex (0..255) - SAI_VS_SWITCH_INDEX_MAX
         * if N is not specified then zero (0) is assumed
         */

        if (line.size() > 0 && (line[0] == '#' || line[0] == ';'))
        {
            continue;
        }

        SWSS_LOG_INFO("line: %s", line.c_str());

        auto toks = swss::tokenize(line, '=');

        if (toks.size() != 2)
        {
            SWSS_LOG_ERROR("expected 2 tokens, got: %zu on line: %s", toks.size(), line.c_str());
            continue;
        }

        auto tokens = swss::tokenize(toks.at(0), ':');

        if (tokens.size() == 2)
        {
            parseLineWithIndex(container, tokens, toks.at(1));
        }
        else if (tokens.size() == 1)
        {
            parseLineWithNoIndex(container, tokens, toks.at(1));
        }
        else
        {
            SWSS_LOG_ERROR("expected 1 or 2 tokens in line %s, got %zu", line.c_str(), tokens.size());
        }
    }

    return container;
}

void ResourceLimiterParser::parseLineWithIndex(
        _In_ std::shared_ptr<ResourceLimiterContainer> container,
        _In_ const std::vector<std::string>& tokens,
        _In_ const std::string& strLimit)
{
    SWSS_LOG_ENTER();

    auto swidx          = tokens.at(0);
    auto strObjectType  = tokens.at(1);

    uint32_t switchIndex;

    if (sscanf(swidx.c_str(), "%u", & switchIndex) != 1)
    {
        SWSS_LOG_ERROR("failed to parse switchIndex: %s", swidx.c_str());
        return;
    }

    parse(container, switchIndex, strObjectType, strLimit);
}

void ResourceLimiterParser::parseLineWithNoIndex(
        _In_ std::shared_ptr<ResourceLimiterContainer> container,
        _In_ const std::vector<std::string>& tokens,
        _In_ const std::string& strLimit)
{
    SWSS_LOG_ENTER();

    auto strObjectType  = tokens.at(0);

    parse(container, ResourceLimiter::DEFAULT_SWITCH_INDEX, strObjectType, strLimit);
}

void ResourceLimiterParser::parse(
        _In_ std::shared_ptr<ResourceLimiterContainer> container,
        _In_ uint32_t switchIndex,
        _In_ const std::string& strObjectType,
        _In_ const std::string& strLimit)
{
    SWSS_LOG_ENTER();

    sai_object_type_t objectType;

    try
    {
        sai_deserialize_object_type(strObjectType, objectType);
    }
    catch(const std::exception& e)
    {
        SWSS_LOG_ERROR("failed to deserialize '%s' as object type: %s", strObjectType.c_str(), e.what());
        return;
    }

    size_t limit;

    if (sscanf(strLimit.c_str(), "%zu", &limit) != 1)
    {
        SWSS_LOG_ERROR("failed to parse '%s' as limit", strLimit.c_str());
        return;
    }

    auto limiter = container->getResourceLimiter(switchIndex);

    if (limiter == nullptr)
    {
        limiter = std::make_shared<ResourceLimiter>(switchIndex);

        container->insert(switchIndex, limiter);
    }

    SWSS_LOG_NOTICE("adding limit on switch index %u, %s = %zu",
            switchIndex,
            sai_serialize_object_type(objectType).c_str(),
            limit);

    limiter->setObjectTypeLimit(objectType, limit);
}
