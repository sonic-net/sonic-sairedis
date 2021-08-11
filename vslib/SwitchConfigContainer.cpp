#include "SwitchConfigContainer.h"

#include "swss/logger.h"

using namespace saivs;

void SwitchConfigContainer::insert(
        _In_ std::shared_ptr<SwitchConfig> config)
{
    SWSS_LOG_ENTER();

    auto it = m_indexToConfig.find(config->m_switchIndex);

    if (it != m_indexToConfig.end())
    {
        SWSS_LOG_THROW("switch with index %u already exists", config->m_switchIndex);
    }

    auto itt = m_hwinfoToConfig.find(config->m_hardwareInfo);

    if (itt != m_hwinfoToConfig.end())
    {
        SWSS_LOG_THROW("switch with hwinfo '%s' already exists",
                config->m_hardwareInfo.c_str());
    }

    SWSS_LOG_NOTICE("added switch: idx %u, hwinfo '%s'",
            config->m_switchIndex,
            config->m_hardwareInfo.c_str());

    m_indexToConfig[config->m_switchIndex] = config;

    m_hwinfoToConfig[config->m_hardwareInfo] = config;
}

std::shared_ptr<SwitchConfig> SwitchConfigContainer::getConfig(
        _In_ uint32_t switchIndex) const
{
    SWSS_LOG_ENTER();

    auto it = m_indexToConfig.find(switchIndex);

    if (it != m_indexToConfig.end())
    {
        return it->second;
    }

    return nullptr;
}

std::shared_ptr<SwitchConfig> SwitchConfigContainer::getConfig(
        _In_ const std::string& hardwareInfo) const
{
    SWSS_LOG_ENTER();

    auto it = m_hwinfoToConfig.find(hardwareInfo);

    if (it != m_hwinfoToConfig.end())
    {
        return it->second;
    }

    return nullptr;
}

std::set<std::shared_ptr<SwitchConfig>> SwitchConfigContainer::getSwitchConfigs() const
{
    SWSS_LOG_ENTER();

    std::set<std::shared_ptr<SwitchConfig>> set;

    for (auto& kvp: m_hwinfoToConfig)
    {
        set.insert(kvp.second);
    }

    return set;
}
