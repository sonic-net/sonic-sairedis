#include "swss/logger.h"
#include "PortMap.h"

/**
 * @brief Port map global map.
 *
 * WARNING: This object must have global declaration and this exact name since
 * external RPC server is linking against this object when in use.
 */
std::map<std::set<int>, std::string> gPortMap;

void PortMap::insert(
        _In_ const std::set<int>& laneSet,
        _In_ const std::string& name)
{
    SWSS_LOG_ENTER();

    m_portMap.insert(std::make_pair(laneSet, name));
}

void PortMap::clear()
{
    SWSS_LOG_ENTER();

    m_portMap.clear();
}

size_t PortMap::size() const
{
    SWSS_LOG_ENTER();

    return m_portMap.size();
}

const std::map<std::set<int>, std::string>& PortMap::getRawPortMap() const
{
    SWSS_LOG_ENTER();

    return m_portMap;
};

void PortMap::setGlobalPortMap(
        _In_ std::shared_ptr<PortMap> portMap)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_NOTICE("setting global gPortMap for rpc server");

    gPortMap = portMap->getRawPortMap();
}

