#include "ContextConfigContainer.h"

#include "swss/logger.h"
#include "swss/json.hpp"

#include <cstring>
#include <fstream>

using json = nlohmann::json;

using namespace saivs;

ContextConfigContainer::ContextConfigContainer()
{
    SWSS_LOG_ENTER();

    // empty
}

ContextConfigContainer::~ContextConfigContainer()
{
    SWSS_LOG_ENTER();

    // empty
}

std::shared_ptr<ContextConfigContainer> ContextConfigContainer::getDefault()
{
    SWSS_LOG_ENTER();

    auto ccc = std::make_shared<ContextConfigContainer>();

    auto cc = std::make_shared<ContextConfig>(0, "VirtualSwitch", "ASIC_DB");

    auto sc = std::make_shared<SwitchConfig>(0, "");

    cc->insert(sc);

    ccc->m_map[0] = cc;

    return ccc;
}

void ContextConfigContainer::insert(
        _In_ std::shared_ptr<ContextConfig> contextConfig)
{
    SWSS_LOG_ENTER();

    m_map[contextConfig->m_guid] = contextConfig;
}

std::shared_ptr<ContextConfig> ContextConfigContainer::get(
        _In_ uint32_t guid)
{
    SWSS_LOG_ENTER();

    auto it = m_map.find(guid);

    if (it == m_map.end())
        return nullptr;

    return it->second;
}

std::shared_ptr<ContextConfigContainer> ContextConfigContainer::loadFromFile(
        _In_ const char* contextConfig)
{
    SWSS_LOG_ENTER();

    auto ccc = std::make_shared<ContextConfigContainer>();

    if (contextConfig == nullptr || strlen(contextConfig) == 0)
    {
        SWSS_LOG_NOTICE("no context config specified, will load default context config");

        return getDefault();
    }

    std::ifstream ifs(contextConfig);

    if (!ifs.good())
    {
        SWSS_LOG_ERROR("failed to read '%s', err: %s, returning default", contextConfig, strerror(errno));

        return getDefault();
    }

    try
    {
        json j;
        ifs >> j;

        for (size_t idx = 0; idx < j["CONTEXTS"].size(); idx++)
        {
            json& item = j["CONTEXTS"][idx];

            uint32_t guid = item["guid"];

            const std::string& name = item["name"];

            const std::string& dbAsic = item["dbAsic"];

            SWSS_LOG_NOTICE("contextConfig: guid: %u, name: %s, dbAsic: %s", guid, name.c_str(), dbAsic.c_str());

            auto cc = std::make_shared<ContextConfig>(guid, name, dbAsic);

            for (size_t k = 0; k < item["switches"].size(); k++)
            {
                json& sw = item["switches"][k];

                uint32_t switchIndex = sw["index"];
                const std::string& hwinfo = sw["hwinfo"];

                auto sc = std::make_shared<SwitchConfig>(switchIndex, hwinfo);

                cc->insert(sc);

                SWSS_LOG_NOTICE("insert into context '%s' config for hwinfo '%s'", cc->m_name.c_str(), hwinfo.c_str());
            }

            ccc->insert(cc);

            SWSS_LOG_NOTICE("insert context '%s' into container list", cc->m_name.c_str());
        }
    }
    catch (const std::exception& e)
    {
        SWSS_LOG_ERROR("Failed to load '%s': %s, returning default", contextConfig, e.what());

        return getDefault();
    }

    SWSS_LOG_NOTICE("loaded %zu context configs", ccc->m_map.size());

    return ccc;
}

std::set<std::shared_ptr<ContextConfig>> ContextConfigContainer::getAllContextConfigs()
{
    SWSS_LOG_ENTER();

    std::set<std::shared_ptr<ContextConfig>> set;

    for (auto&item: m_map)
    {
        set.insert(item.second);
    }

    return set;
}
