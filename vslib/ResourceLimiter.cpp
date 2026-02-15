#include "ResourceLimiter.h"

#include "swss/logger.h"
#include "meta/sai_serialize.h"

using namespace saivs;

ResourceLimiter::ResourceLimiter(
        _In_ uint32_t switchIndex):
    m_switchIndex(switchIndex)
{
    SWSS_LOG_ENTER();

    // empty
}

size_t ResourceLimiter::getObjectTypeLimit(
        _In_ sai_object_type_t objectType) const
{
    SWSS_LOG_ENTER();

    auto it = m_objectTypeLimits.find(objectType);

    if (it != m_objectTypeLimits.end())
    {
        return it->second;
    }

    // default limit is maximum

    return SIZE_MAX;
}

void ResourceLimiter::setObjectTypeLimit(
        _In_ sai_object_type_t objectType,
        _In_ size_t limit)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_INFO("setting %s limit to %zu",
            sai_serialize_object_type(objectType).c_str(),
            limit);

    m_objectTypeLimits[objectType] = limit;
}

void ResourceLimiter::removeObjectTypeLimit(
        _In_ sai_object_type_t objectType)
{
    SWSS_LOG_ENTER();

    m_objectTypeLimits[objectType] = SIZE_MAX;
}

void ResourceLimiter::clearLimits()
{
    SWSS_LOG_ENTER();

    m_objectTypeLimits.clear();
}
