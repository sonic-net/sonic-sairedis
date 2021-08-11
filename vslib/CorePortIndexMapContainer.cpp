#include "CorePortIndexMapContainer.h"

#include "swss/logger.h"

using namespace saivs;

void CorePortIndexMapContainer::insert(
        _In_ std::shared_ptr<CorePortIndexMap> corePortIndexMap)
{
    SWSS_LOG_ENTER();

    m_map[corePortIndexMap->getSwitchIndex()] = corePortIndexMap;
}

void CorePortIndexMapContainer::remove(
        _In_ uint32_t switchIndex)
{
    SWSS_LOG_ENTER();

    auto it = m_map.find(switchIndex);

    if (it != m_map.end())
    {
        m_map.erase(it);
    }
}

std::shared_ptr<CorePortIndexMap> CorePortIndexMapContainer::getCorePortIndexMap(
        _In_ uint32_t switchIndex) const
{
    SWSS_LOG_ENTER();

    auto it = m_map.find(switchIndex);

    if (it == m_map.end())
    {
        return nullptr;
    }

    return it->second;
}

void CorePortIndexMapContainer::clear()
{
    SWSS_LOG_ENTER();

    m_map.clear();
}

bool CorePortIndexMapContainer::hasCorePortIndexMap(
        _In_ uint32_t switchIndex) const
{
    SWSS_LOG_ENTER();

    return m_map.find(switchIndex) != m_map.end();
}

size_t CorePortIndexMapContainer::size() const
{
    SWSS_LOG_ENTER();

    return m_map.size();
}

void CorePortIndexMapContainer::removeEmptyCorePortIndexMaps()
{
    SWSS_LOG_ENTER();

    for (auto it = m_map.begin(); it != m_map.end();)
    {
        if (it->second->isEmpty())
        {
            it = m_map.erase(it);
        }
        else
        {
            ++it;
        }
    }
}
