#include "ResourceLimiterContainer.h"

#include "swss/logger.h"

using namespace saivs;

void ResourceLimiterContainer::insert(
        _In_ uint32_t switchIndex,
        _In_ std::shared_ptr<ResourceLimiter> rl)
{
    SWSS_LOG_ENTER();

    if (rl == nullptr)
    {
        SWSS_LOG_THROW("resouorce limitter pointer can't be nullptr");
    }

    m_container[switchIndex] = rl;
}

void ResourceLimiterContainer::remove(
        _In_ uint32_t switchIndex)
{
    SWSS_LOG_ENTER();

    auto it = m_container.find(switchIndex);

    if (it != m_container.end())
    {
        m_container.erase(it);
    }
}

std::shared_ptr<ResourceLimiter> ResourceLimiterContainer::getResourceLimiter(
        _In_ uint32_t switchIndex) const
{
    SWSS_LOG_ENTER();

    auto it = m_container.find(switchIndex);

    if (it != m_container.end())
    {
        return it->second;
    }

    return nullptr;
}

void ResourceLimiterContainer::clear()
{
    SWSS_LOG_ENTER();

    m_container.clear();
}
