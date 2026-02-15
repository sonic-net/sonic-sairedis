#include "BreakConfig.h"

#include "swss/logger.h"

using namespace syncd;

void BreakConfig::insert(
        _In_ sai_object_type_t objectType)
{
    SWSS_LOG_ENTER();

    m_set.insert(objectType);
}

void BreakConfig::remove(
        _In_ sai_object_type_t objectType)
{
    SWSS_LOG_ENTER();

    auto it = m_set.find(objectType);

    if (it != m_set.end())
    {
        m_set.erase(it);
    }
}

void BreakConfig::clear()
{
    SWSS_LOG_ENTER();

    m_set.clear();
}

bool BreakConfig::shouldBreakBeforeMake(
        _In_ sai_object_type_t objectType) const
{
    SWSS_LOG_ENTER();

    return m_set.find(objectType) != m_set.end();
}

size_t BreakConfig::size() const
{
    SWSS_LOG_ENTER();

    return m_set.size();
}

