#include "FlexCounterManager.h"

#include "swss/logger.h"

using namespace syncd;

#define MUTEX std::lock_guard<std::mutex> _lock(m_mutex);

FlexCounterManager::FlexCounterManager(
        _In_ std::shared_ptr<sairedis::SaiInterface> vendorSai,
        _In_ const std::string& dbCounters):
    m_vendorSai(vendorSai),
    m_dbCounters(dbCounters)
{
    SWSS_LOG_ENTER();

    // empty
}

std::shared_ptr<FlexCounter> FlexCounterManager::getInstance(
        _In_ const std::string& instanceId)
{
    MUTEX;

    SWSS_LOG_ENTER();

    if (m_flexCounters.count(instanceId) == 0)
    {
        auto counter = std::make_shared<FlexCounter>(instanceId, m_vendorSai, m_dbCounters);

        m_flexCounters[instanceId] = counter;
    }

    return m_flexCounters.at(instanceId);
}

void FlexCounterManager::removeInstance(
        _In_ const std::string& instanceId)
{
    MUTEX;

    SWSS_LOG_ENTER();

    m_flexCounters.erase(instanceId);
}

void FlexCounterManager::removeAllCounters()
{
    MUTEX;

    SWSS_LOG_ENTER();

    m_flexCounters.clear();
}

void FlexCounterManager::removeCounterPlugins(
        _In_ const std::string& instanceId)
{
    SWSS_LOG_ENTER();

    auto fc = getInstance(instanceId);

    fc->removeCounterPlugins();

    if (fc->isDiscarded())
    {
        removeInstance(instanceId);
    }
}

void FlexCounterManager::addCounterPlugin(
        _In_ const std::string& instanceId,
        _In_ const std::vector<swss::FieldValueTuple>& values)
{
    SWSS_LOG_ENTER();

    auto fc = getInstance(instanceId);

    fc->addCounterPlugin(values);
}

void FlexCounterManager::addCounter(
        _In_ sai_object_id_t vid,
        _In_ sai_object_id_t rid,
        _In_ const std::string& instanceId,
        _In_ const std::vector<swss::FieldValueTuple>& values)
{
    SWSS_LOG_ENTER();

    auto fc = getInstance(instanceId);

    fc->addCounter(vid, rid, values);

    if (fc->isDiscarded())
    {
        removeInstance(instanceId);
    }
}

void FlexCounterManager::removeCounter(
        _In_ sai_object_id_t vid,
        _In_ const std::string& instanceId)
{
    SWSS_LOG_ENTER();

    auto fc = getInstance(instanceId);

    fc->removeCounter(vid);

    if (fc->isDiscarded())
    {
        removeInstance(instanceId);
    }
}

