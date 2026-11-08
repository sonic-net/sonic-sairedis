#include "SwitchContainer.h"

#include "swss/logger.h"
#include "meta/sai_serialize.h"

using namespace sairedis;

void SwitchContainer::insert(
        _In_ std::shared_ptr<Switch> sw)
{
    SWSS_LOG_ENTER();

    auto switchId = sw->getSwitchId();

    if (m_switchMap.find(switchId) != m_switchMap.end())
    {
        SWSS_LOG_THROW("switch %s already exist in container",
                sai_serialize_object_id(switchId).c_str());
    }

    // NOTE: this check should be also checked by metadata and return SAI
    // failure before calling this constructor
    if (getSwitchByHardwareInfo(sw->getHardwareInfo()))
    {
        SWSS_LOG_THROW("switch with hardware info '%s' already exists in container!",
                sw->getHardwareInfo().c_str());
    }

    m_switchMap[switchId] = sw;
}

void SwitchContainer::removeSwitch(
        _In_ sai_object_id_t switchId)
{
    SWSS_LOG_ENTER();

    auto it = m_switchMap.find(switchId);

    if (it == m_switchMap.end())
    {
        SWSS_LOG_THROW("switch %s not present in container",
                sai_serialize_object_id(switchId).c_str());
    }

    m_switchMap.erase(it);
}

void SwitchContainer::removeSwitch(
        _In_ std::shared_ptr<Switch> sw)
{
    SWSS_LOG_ENTER();

    removeSwitch(sw->getSwitchId());
}

std::shared_ptr<Switch> SwitchContainer::getSwitch(
        _In_ sai_object_id_t switchId) const
{
    SWSS_LOG_ENTER();

    auto it = m_switchMap.find(switchId);

    if (it == m_switchMap.end())
        return nullptr;

    return it->second;
}

void SwitchContainer::clear()
{
    SWSS_LOG_ENTER();

    m_switchMap.clear();
}

bool SwitchContainer::contains(
        _In_ sai_object_id_t switchId) const
{
    SWSS_LOG_ENTER();

    return m_switchMap.find(switchId) != m_switchMap.end();
}

std::shared_ptr<Switch> SwitchContainer::getSwitchByHardwareInfo(
        _In_ const std::string& hardwareInfo) const
{
    SWSS_LOG_ENTER();

    for (auto&kvp: m_switchMap)
    {
        if (kvp.second->getHardwareInfo() == hardwareInfo)
            return kvp.second;
    }

    return nullptr;
}

