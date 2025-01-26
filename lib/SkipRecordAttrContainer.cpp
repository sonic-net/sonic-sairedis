#include "SkipRecordAttrContainer.h"

extern "C" {
#include "otaimetadata.h"
}

#include "swss/logger.h"

using namespace otairedis;

SkipRecordAttrContainer::SkipRecordAttrContainer()
{
    SWSS_LOG_ENTER();

    // default set of attributes to skip recording

    //add(OTAI_OBJECT_TYPE_SWITCH, OTAI_SWITCH_ATTR_AVAILABLE_IPV4_ROUTE_ENTRY);
    //add(OTAI_OBJECT_TYPE_SWITCH, OTAI_SWITCH_ATTR_AVAILABLE_IPV6_ROUTE_ENTRY);
    //add(OTAI_OBJECT_TYPE_SWITCH, OTAI_SWITCH_ATTR_AVAILABLE_IPV4_NEXTHOP_ENTRY);
    //add(OTAI_OBJECT_TYPE_SWITCH, OTAI_SWITCH_ATTR_AVAILABLE_IPV6_NEXTHOP_ENTRY);
    //add(OTAI_OBJECT_TYPE_SWITCH, OTAI_SWITCH_ATTR_AVAILABLE_IPV4_NEIGHBOR_ENTRY);
    //add(OTAI_OBJECT_TYPE_SWITCH, OTAI_SWITCH_ATTR_AVAILABLE_IPV6_NEIGHBOR_ENTRY);
    //add(OTAI_OBJECT_TYPE_SWITCH, OTAI_SWITCH_ATTR_AVAILABLE_NEXT_HOP_GROUP_ENTRY);
    //add(OTAI_OBJECT_TYPE_SWITCH, OTAI_SWITCH_ATTR_AVAILABLE_NEXT_HOP_GROUP_MEMBER_ENTRY);
    //add(OTAI_OBJECT_TYPE_SWITCH, OTAI_SWITCH_ATTR_AVAILABLE_FDB_ENTRY);
    //add(OTAI_OBJECT_TYPE_SWITCH, OTAI_SWITCH_ATTR_AVAILABLE_L2MC_ENTRY);
    //add(OTAI_OBJECT_TYPE_SWITCH, OTAI_SWITCH_ATTR_AVAILABLE_IPMC_ENTRY);
    //add(OTAI_OBJECT_TYPE_SWITCH, OTAI_SWITCH_ATTR_AVAILABLE_SNAT_ENTRY);
    //add(OTAI_OBJECT_TYPE_SWITCH, OTAI_SWITCH_ATTR_AVAILABLE_DNAT_ENTRY);
    //add(OTAI_OBJECT_TYPE_SWITCH, OTAI_SWITCH_ATTR_AVAILABLE_DOUBLE_NAT_ENTRY);
    //add(OTAI_OBJECT_TYPE_SWITCH, OTAI_SWITCH_ATTR_AVAILABLE_ACL_TABLE);
    //add(OTAI_OBJECT_TYPE_SWITCH, OTAI_SWITCH_ATTR_AVAILABLE_ACL_TABLE_GROUP);
    //add(OTAI_OBJECT_TYPE_SWITCH, OTAI_SWITCH_ATTR_TEMP_LIST);
    //add(OTAI_OBJECT_TYPE_SWITCH, OTAI_SWITCH_ATTR_MAX_TEMP);
    //add(OTAI_OBJECT_TYPE_SWITCH, OTAI_SWITCH_ATTR_AVERAGE_TEMP);

    //add(OTAI_OBJECT_TYPE_ACL_TABLE, OTAI_ACL_TABLE_ATTR_AVAILABLE_ACL_ENTRY);
    //add(OTAI_OBJECT_TYPE_ACL_TABLE, OTAI_ACL_TABLE_ATTR_AVAILABLE_ACL_COUNTER);

    //add(OTAI_OBJECT_TYPE_PORT, OTAI_PORT_ATTR_FABRIC_ATTACHED);
    //add(OTAI_OBJECT_TYPE_PORT, OTAI_PORT_ATTR_FABRIC_ATTACHED_SWITCH_ID);
    //add(OTAI_OBJECT_TYPE_PORT, OTAI_PORT_ATTR_FABRIC_ATTACHED_PORT_INDEX);
}

bool SkipRecordAttrContainer::add(
        _In_ otai_object_type_t objectType,
        _In_ otai_attr_id_t attrId)
{
    SWSS_LOG_ENTER();

    auto md = otai_metadata_get_attr_metadata(objectType, attrId);

    if (md == NULL)
    {
        SWSS_LOG_WARN("failed to get metadata for %d:%d", objectType, attrId);

        return false;
    }

    if (md->isoidattribute)
    {
        SWSS_LOG_WARN("%s is OID attribute, will not add to container", md->attridname);

        return false;
    }

    m_map[objectType].insert(attrId);

    SWSS_LOG_DEBUG("added %s to container", md->attridname);

    return true;
}

bool SkipRecordAttrContainer::remove(
        _In_ otai_object_type_t objectType,
        _In_ otai_attr_id_t attrId)
{
    SWSS_LOG_ENTER();

    auto it = m_map.find(objectType);

    if (it == m_map.end())
    {
        return false;
    }

    auto its = it->second.find(attrId);

    if (its == it->second.end())
    {
        return false;
    }

    it->second.erase(its);

    return true;
}

void SkipRecordAttrContainer::clear()
{
    SWSS_LOG_ENTER();

    m_map.clear();
}

bool SkipRecordAttrContainer::canSkipRecording(
        _In_ otai_object_type_t objectType,
        _In_ uint32_t count,
        _In_ otai_attribute_t* attrList) const
{
    SWSS_LOG_ENTER();

    if (count == 0)
        return false;

    if (!attrList)
        return false;

    auto it = m_map.find(objectType);

    if (it == m_map.end())
        return false;

    // we can skip if all attributes on list are present in container

    auto& set = it->second;

    for (uint32_t idx = 0; idx < count; idx++)
    {
        if (set.find(attrList[idx].id) == set.end())
            return false;
    }

    return true;
}
