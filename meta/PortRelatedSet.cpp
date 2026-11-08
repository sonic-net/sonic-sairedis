#include "PortRelatedSet.h"

#include "swss/logger.h"

using namespace saimeta;

void PortRelatedSet::insert(
        _In_ sai_object_id_t portId,
        _In_ sai_object_id_t relatedObjectId)
{
    SWSS_LOG_ENTER();

    if (relatedObjectId == SAI_NULL_OBJECT_ID)
    {
        return;
    }

    if (portId == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_THROW("portId is NULL");
    }

    m_mapset[portId].insert(relatedObjectId);
}

const std::set<sai_object_id_t> PortRelatedSet::getPortRelatedObjects(
        _In_ sai_object_id_t portId) const
{
    SWSS_LOG_ENTER();

    auto it = m_mapset.find(portId);

    if (it != m_mapset.end())
    {
        return it->second;
    }

    return { };
}

void PortRelatedSet::clear()
{
    SWSS_LOG_ENTER();

    m_mapset.clear();
}

void PortRelatedSet::removePort(
        _In_ sai_object_id_t portId)
{
    SWSS_LOG_ENTER();

    auto it = m_mapset.find(portId);

    if (it != m_mapset.end())
    {
        m_mapset.erase(it);
    }
}

std::vector<sai_object_id_t> PortRelatedSet::getAllPorts() const
{
    SWSS_LOG_ENTER();

    std::vector<sai_object_id_t> vec;

    for (auto& it: m_mapset)
    {
        vec.push_back(it.first);
    }

    return vec;
}
