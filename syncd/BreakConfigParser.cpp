#include "BreakConfigParser.h"

#include "swss/logger.h"

#include "meta/sai_serialize.h"

using namespace syncd;

std::shared_ptr<BreakConfig> BreakConfigParser::parseBreakConfig(
        _In_ const std::string& filePath)
{
    SWSS_LOG_ENTER();

    auto config = std::make_shared<BreakConfig>();

    if (filePath.size() == 0)
    {
        return config; // return empty config
    }

    std::ifstream file(filePath);

    if (!file.is_open())
    {
        SWSS_LOG_ERROR("failed to open break config file: %s: errno: %s, returning empty config", filePath.c_str(), strerror(errno));

        return config;
    }

    std::string line;

    while (getline(file, line))
    {
        if (line.size() > 0 && (line[0] == '#' || line[0] == ';'))
        {
            continue;
        }

        sai_object_type_t objectType;

        try
        {
            sai_deserialize_object_type(line, objectType);

            config->insert(objectType);

            SWSS_LOG_INFO("inserting %s to break config", line.c_str());
        }
        catch (const std::exception& e)
        {
            SWSS_LOG_WARN("failed to parse '%s' as sai_object_type_t", line.c_str());
        }
    }

    SWSS_LOG_NOTICE("break config parse success, contains %zu entries", config->size());


    return config;
}
