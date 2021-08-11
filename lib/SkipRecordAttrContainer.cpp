#include "SkipRecordAttrContainer.h"

extern "C" {
#include "saimetadata.h"
}

#include "swss/logger.h"

using namespace sairedis;

SkipRecordAttrContainer::SkipRecordAttrContainer()
{
    SWSS_LOG_ENTER();

    // default set of attributes to skip recording

    add(SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_AVAILABLE_IPV4_ROUTE_ENTRY);
    add(SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_AVAILABLE_IPV6_ROUTE_ENTRY);
    add(SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_AVAILABLE_IPV4_NEXTHOP_ENTRY);
    add(SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_AVAILABLE_IPV6_NEXTHOP_ENTRY);
    add(SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_AVAILABLE_IPV4_NEIGHBOR_ENTRY);
    add(SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_AVAILABLE_IPV6_NEIGHBOR_ENTRY);
    add(SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_AVAILABLE_NEXT_HOP_GROUP_ENTRY);
    add(SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_AVAILABLE_NEXT_HOP_GROUP_MEMBER_ENTRY);
    add(SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_AVAILABLE_FDB_ENTRY);
    add(SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_AVAILABLE_L2MC_ENTRY);
    add(SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_AVAILABLE_IPMC_ENTRY);
    add(SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_AVAILABLE_SNAT_ENTRY);
    add(SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_AVAILABLE_DNAT_ENTRY);
    add(SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_AVAILABLE_DOUBLE_NAT_ENTRY);
    add(SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_AVAILABLE_ACL_TABLE);
    add(SAI_OBJECT_TYPE_SWITCH, SAI_SWITCH_ATTR_AVAILABLE_ACL_TABLE_GROUP);

    add(SAI_OBJECT_TYPE_ACL_TABLE, SAI_ACL_TABLE_ATTR_AVAILABLE_ACL_ENTRY);
    add(SAI_OBJECT_TYPE_ACL_TABLE, SAI_ACL_TABLE_ATTR_AVAILABLE_ACL_COUNTER);
}

bool SkipRecordAttrContainer::add(
        _In_ sai_object_type_t objectType,
        _In_ sai_attr_id_t attrId)
{
    SWSS_LOG_ENTER();

    auto md = sai_metadata_get_attr_metadata(objectType, attrId);

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
        _In_ sai_object_type_t objectType,
        _In_ sai_attr_id_t attrId)
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
        _In_ sai_object_type_t objectType,
        _In_ uint32_t count,
        _In_ sai_attribute_t* attrList) const
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
