#include "SaiSwitchInterface.h"

#include "swss/logger.h"

using namespace syncd;

SaiSwitchInterface::SaiSwitchInterface(
        _In_ sai_object_id_t switchVid,
        _In_ sai_object_id_t switchRid):
    m_switch_vid(switchVid),
    m_switch_rid(switchRid)
{
    SWSS_LOG_ENTER();

    // empty
}

sai_object_id_t SaiSwitchInterface::getVid() const
{
    SWSS_LOG_ENTER();

    return m_switch_vid;
}

sai_object_id_t SaiSwitchInterface::getRid() const
{
    SWSS_LOG_ENTER();

    return m_switch_rid;
}

sai_object_id_t SaiSwitchInterface::getSwitchDefaultAttrOid(
        _In_ sai_attr_id_t attr_id) const
{
    SWSS_LOG_ENTER();

    auto it = m_default_rid_map.find(attr_id);

    if (it == m_default_rid_map.end())
    {
        auto meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_SWITCH, attr_id);

        const char* name = (meta) ? meta->attridname : "UNKNOWN";

        SWSS_LOG_THROW("attribute %s (%d) not found in default RID map", name, attr_id);
    }

    return it->second;
}

std::set<sai_object_id_t> SaiSwitchInterface::getWarmBootNewDiscoveredVids()
{
    SWSS_LOG_ENTER();

    return m_warmBootNewDiscoveredVids;
}

