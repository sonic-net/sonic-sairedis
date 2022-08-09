#include "SaiSwitchAsic.h"

#include "syncd/VidManager.h"

#include "meta/sai_serialize.h"

#include "swss/logger.h"

#pragma GCC diagnostic ignored "-Wsuggest-attribute=noreturn"

using namespace saiasiccmp;

SaiSwitchAsic::SaiSwitchAsic(
        _In_ sai_object_id_t switchVid,
        _In_ sai_object_id_t switchRid,
        _In_ const std::map<sai_object_id_t, sai_object_id_t> vid2rid,
        _In_ const std::map<sai_object_id_t, sai_object_id_t> rid2vid,
        _In_ const std::map<std::string, sai_object_id_t>& hidden,
        _In_ const std::map<sai_object_id_t, sai_object_type_t>& coldVids):
    SaiSwitchInterface(switchVid, switchRid)
{
    SWSS_LOG_ENTER();

    m_vid2rid = vid2rid;
    m_rid2vid = rid2vid;

    // set default rid map

    for (auto it: hidden)
    {
        auto meta = sai_metadata_get_attr_metadata_by_attr_id_name(it.first.c_str());

        if (meta == nullptr)
        {
            SWSS_LOG_THROW("can't find attr metadata for %s", it.first.c_str());
        }

        m_default_rid_map[meta->attrid] = it.second;
    }

    // populate cold vids

    for (auto it: coldVids)
    {
        m_coldBootDiscoveredVids.insert(it.first);
    }
}

std::unordered_map<sai_object_id_t, sai_object_id_t> SaiSwitchAsic::getVidToRidMap() const
{
    SWSS_LOG_ENTER();

    std::unordered_map<sai_object_id_t, sai_object_id_t> filtered;

    for (auto& v2r: m_vid2rid)
    {
        auto switchId = syncd::VidManager::switchIdQuery(v2r.first);

        if (switchId == m_switch_vid)
        {
            filtered[v2r.first] = v2r.second;
        }
    }

    return filtered;
}

std::unordered_map<sai_object_id_t, sai_object_id_t> SaiSwitchAsic::getRidToVidMap() const
{
    SWSS_LOG_ENTER();

    // since maps can contain multiple switches, filter only this switch

    std::unordered_map<sai_object_id_t, sai_object_id_t> filtered;

    for (auto& r2v: m_rid2vid)
    {
        auto switchId = syncd::VidManager::switchIdQuery(r2v.second);

        if (switchId == m_switch_vid)
        {
            filtered[r2v.first] = r2v.second;
        }
    }

    return filtered;
}

bool SaiSwitchAsic::isDiscoveredRid(
        _In_ sai_object_id_t rid) const
{
    SWSS_LOG_ENTER();

    SWSS_LOG_WARN("FIXME, VID: %s", sai_serialize_object_id(m_rid2vid.at(rid)).c_str());

    return true;
}

bool SaiSwitchAsic::isColdBootDiscoveredRid(
        _In_ sai_object_id_t rid) const
{
    SWSS_LOG_ENTER();

    auto coldBootDiscoveredVids = getColdBootDiscoveredVids();

    /*
     * If object was discovered in cold boot, it must have valid RID assigned,
     * except objects that were removed like VLAN_MEMBER.
     */

    auto vid = m_rid2vid.at(rid);

    return coldBootDiscoveredVids.find(vid) != coldBootDiscoveredVids.end();
}

bool SaiSwitchAsic::isSwitchObjectDefaultRid(
        _In_ sai_object_id_t rid) const
{
    SWSS_LOG_ENTER();

    for (const auto &p: m_default_rid_map)
    {
        if (p.second == rid)
        {
            return true;
        }
    }

    return false;
}

bool SaiSwitchAsic::isNonRemovableRid(
        _In_ sai_object_id_t rid) const
{
    SWSS_LOG_ENTER();

    if (rid == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_THROW("NULL rid passed");
    }

    /*
     * Check for SAI_SWITCH_ATTR_DEFAULT_* oids like cpu, default virtual
     * router.  Those objects can't be removed if user ask for it.
     */

    /*
     * Here we are checking for isSwitchObjectDefaultRid first then
     * ColdBootDiscoveredRid as it is possible we can discover switch Internal
     * OID as part of warm-boot also especially when we are doing SAI upgrade
     * as part of warm-boot.
     * */

    if (isSwitchObjectDefaultRid(rid))
    {
        return true;
    }

    if (!isColdBootDiscoveredRid(rid))
    {
        /*
         * This object was not discovered on cold boot so it can be removed.
         */

        return false;
    }

    auto vid = m_rid2vid.at(rid);

    auto ot = syncd::VidManager::objectTypeQuery(vid);

    switch (ot)
    {
        case SAI_OBJECT_TYPE_VLAN_MEMBER:
        case SAI_OBJECT_TYPE_STP_PORT:
        case SAI_OBJECT_TYPE_BRIDGE_PORT:

            /*
             * Those objects were discovered during cold boot, but they can
             * still be removed since switch allows that.
             */

            return false;

        case SAI_OBJECT_TYPE_PORT:
        case SAI_OBJECT_TYPE_QUEUE:
        case SAI_OBJECT_TYPE_INGRESS_PRIORITY_GROUP:
        case SAI_OBJECT_TYPE_SCHEDULER_GROUP:
        case SAI_OBJECT_TYPE_HASH:

            /*
             * TODO: Some vendors support removing of those objects then we
             * need to came up with different approach. Probably SaiSwitch
             * will need to decide whether it's possible to remove object.
             */

            return true;

        default:
            break;
    }

    SWSS_LOG_WARN("can't determine wheter object %s RID %s can be removed, FIXME",
            sai_serialize_object_type(ot).c_str(),
            sai_serialize_object_id(rid).c_str());

    return true;
}

std::set<sai_object_id_t> SaiSwitchAsic::getDiscoveredRids() const
{
    SWSS_LOG_ENTER();

    SWSS_LOG_WARN("FIXME");

    return {};
}

void SaiSwitchAsic::removeExistingObject(
        _In_ sai_object_id_t rid)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_THROW("not implemented");
}

void SaiSwitchAsic::removeExistingObjectReference(
        _In_ sai_object_id_t rid)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_THROW("not implemented");
}

void SaiSwitchAsic::getDefaultMacAddress(
        _Out_ sai_mac_t& mac) const
{
    SWSS_LOG_ENTER();

    SWSS_LOG_THROW("not implemented");
}

sai_object_id_t SaiSwitchAsic::getDefaultValueForOidAttr(
        _In_ sai_object_id_t rid,
        _In_ sai_attr_id_t attr_id)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_THROW("not implemented");
}

std::set<sai_object_id_t> SaiSwitchAsic::getColdBootDiscoveredVids() const
{
    SWSS_LOG_ENTER();

    if (m_coldBootDiscoveredVids.size() != 0)
    {
        return m_coldBootDiscoveredVids;
    }

    SWSS_LOG_THROW("cold boot vids empty");
}

std::set<sai_object_id_t> SaiSwitchAsic::getWarmBootDiscoveredVids() const
{
    SWSS_LOG_ENTER();

    SWSS_LOG_WARN("FIXME");

    return {};
}

void SaiSwitchAsic::onPostPortCreate(
        _In_ sai_object_id_t port_rid,
        _In_ sai_object_id_t port_vid)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_THROW("not implemented");
}

void SaiSwitchAsic::postPortRemove(
        _In_ sai_object_id_t portRid)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_THROW("not implemented");
}

void SaiSwitchAsic::collectPortRelatedObjects(
        _In_ sai_object_id_t portRid)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_THROW("not implemented");
}
