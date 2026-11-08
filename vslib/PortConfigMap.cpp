#include "PortConfigMap.h"

#include "swss/logger.h"

using namespace saivs;

bool PortConfigMap::add(
        _In_ const std::set<uint32_t>& lanes,
        _In_ const std::string& name)
{
    SWSS_LOG_ENTER();

    if (lanes.empty() || name.empty())
    {
        SWSS_LOG_WARN("cannot add empty port config entry for %s", name.c_str());
        return false;
    }

    auto result = m_lanes_to_name.emplace(lanes, name);
    if (!result.second)
    {
        SWSS_LOG_WARN("duplicate port config lane set for %s and %s",
                result.first->second.c_str(), name.c_str());
        return false;
    }

    return true;
}

std::string PortConfigMap::getPortName(
        _In_ const std::set<uint32_t>& lanes) const
{
    SWSS_LOG_ENTER();

    auto it = m_lanes_to_name.find(lanes);
    if (it == m_lanes_to_name.end())
    {
        return "";
    }

    return it->second;
}

size_t PortConfigMap::size() const
{
    SWSS_LOG_ENTER();

    return m_lanes_to_name.size();
}