#include "VppInterfaceRegistry.h"

#include "swss/logger.h"

using namespace saivs;

std::shared_ptr<VppPhysicalPort> VppInterfaceRegistry::addPhysicalPort(
        _In_ const std::string& hwifName,
        _In_ const std::string& sonicName,
        _In_ sai_object_id_t oid)
{
    SWSS_LOG_ENTER();

    if (oid == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_ERROR("refusing to register port %s with a null oid", sonicName.c_str());

        return nullptr;
    }

    auto existing = findByHwif(hwifName);

    if (existing)
    {
        SWSS_LOG_WARN("interface %s already registered as type %d, not adding physical port",
                hwifName.c_str(), static_cast<int>(existing->getType()));

        return nullptr;
    }

    auto rec = std::make_shared<VppPhysicalPort>(hwifName, sonicName);

    indexRecord(rec);

    setOid(hwifName, oid);

    SWSS_LOG_INFO("registered physical port %s as hwif %s oid 0x%llx",
            sonicName.c_str(), hwifName.c_str(), static_cast<unsigned long long>(oid));

    return rec;
}

std::shared_ptr<VppBondInterface> VppInterfaceRegistry::addLag(
        _In_ uint32_t bondId,
        _In_ uint32_t swIfIndex,
        _In_ sai_object_id_t oid)
{
    SWSS_LOG_ENTER();

    auto hwifName = VppBondInterface::hwifNameFor(bondId);

    auto existing = findByHwif(hwifName);

    if (existing)
    {
        SWSS_LOG_WARN("interface %s already registered, not adding LAG", hwifName.c_str());

        return nullptr;
    }

    if (oid == SAI_NULL_OBJECT_ID || swIfIndex == VppInterface::SWIF_INDEX_INVALID)
    {
        SWSS_LOG_ERROR("refusing to register LAG %s with oid 0x%llx / sw_if_index %u: both are known"
                " at bond creation and must be supplied",
                hwifName.c_str(), static_cast<unsigned long long>(oid), swIfIndex);

        return nullptr;
    }

    auto rec = std::make_shared<VppBondInterface>(bondId);

    indexRecord(rec);

    bindSwIfIndex(hwifName, swIfIndex);

    setOid(hwifName, oid);

    SWSS_LOG_INFO("registered LAG %s sw_if_index %u oid 0x%llx",
            hwifName.c_str(), swIfIndex, static_cast<unsigned long long>(oid));

    return rec;
}

std::shared_ptr<VppSubInterface> VppInterfaceRegistry::addSubInterface(
        _In_ const std::shared_ptr<VppInterface>& parent,
        _In_ uint32_t subId,
        _In_ uint16_t vlanId)
{
    SWSS_LOG_ENTER();

    if (!parent)
    {
        SWSS_LOG_ERROR("cannot add sub-interface .%u with no parent", subId);

        return nullptr;
    }

    if (!findByHwif(parent->getHwifName()))
    {
        SWSS_LOG_ERROR("parent %s of sub-interface .%u is not registered",
                parent->getHwifName().c_str(), subId);

        return nullptr;
    }

    auto hwifName = VppSubInterface::hwifNameFor(parent->getHwifName(), subId);

    auto existing = findByHwif(hwifName);

    if (existing)
    {
        auto sub = std::dynamic_pointer_cast<VppSubInterface>(existing);

        if (!sub)
        {
            SWSS_LOG_ERROR("interface %s already registered as type %d, not a sub-interface",
                    hwifName.c_str(), static_cast<int>(existing->getType()));

            return nullptr;
        }

        SWSS_LOG_INFO("sub-interface %s already registered", hwifName.c_str());

        return sub;
    }

    auto rec = std::make_shared<VppSubInterface>(parent, subId, vlanId);

    indexRecord(rec);

    SWSS_LOG_INFO("registered sub-interface %s vlan %u",
            hwifName.c_str(), static_cast<unsigned int>(vlanId));

    return rec;
}

std::shared_ptr<VppVlanInterface> VppInterfaceRegistry::addBvi(
        _In_ uint16_t vlanId)
{
    SWSS_LOG_ENTER();

    auto hwifName = VppVlanInterface::hwifNameFor(vlanId);

    if (findByHwif(hwifName))
    {
        SWSS_LOG_WARN("interface %s already registered, not adding BVI", hwifName.c_str());

        return nullptr;
    }

    auto rec = std::make_shared<VppVlanInterface>(vlanId);

    indexRecord(rec);

    SWSS_LOG_INFO("registered BVI %s", hwifName.c_str());

    return rec;
}

std::shared_ptr<VppTunnelInterface> VppInterfaceRegistry::addTunnel(
        _In_ const std::string& hwifName,
        _In_ uint32_t vni)
{
    SWSS_LOG_ENTER();

    if (findByHwif(hwifName))
    {
        SWSS_LOG_WARN("interface %s already registered, not adding tunnel", hwifName.c_str());

        return nullptr;
    }

    auto rec = std::make_shared<VppTunnelInterface>(hwifName, vni);

    indexRecord(rec);

    SWSS_LOG_INFO("registered tunnel %s vni %u", hwifName.c_str(), vni);

    return rec;
}

bool VppInterfaceRegistry::bindSwIfIndex(
        _In_ const std::string& hwifName,
        _In_ uint32_t swIfIndex)
{
    SWSS_LOG_ENTER();

    auto rec = findByHwif(hwifName);

    if (!rec)
    {
        SWSS_LOG_ERROR("cannot bind sw_if_index %u: %s is not registered", swIfIndex, hwifName.c_str());

        return false;
    }

    if (swIfIndex == VppInterface::SWIF_INDEX_INVALID)
    {
        SWSS_LOG_ERROR("refusing to bind invalid sw_if_index to %s", hwifName.c_str());

        return false;
    }

    /*
     * VPP reuses sw_if_index values once an interface is deleted. If a stale
     * record still claims this index, drop its binding now; leaving it would
     * mis-attribute every subsequent FDB event to the dead interface.
     */
    auto it = m_bySwIfIndex.find(swIfIndex);

    if (it != m_bySwIfIndex.end() && it->second != rec)
    {
        SWSS_LOG_WARN("sw_if_index %u was still bound to %s, rebinding to %s",
                swIfIndex, it->second->getHwifName().c_str(), hwifName.c_str());

        it->second->m_swIfIndex = VppInterface::SWIF_INDEX_INVALID;

        m_bySwIfIndex.erase(it);
    }

    if (rec->hasSwIfIndex() && rec->getSwIfIndex() != swIfIndex)
    {
        m_bySwIfIndex.erase(rec->getSwIfIndex());
    }

    rec->m_swIfIndex = swIfIndex;

    m_bySwIfIndex[swIfIndex] = rec;

    SWSS_LOG_INFO("bound %s to sw_if_index %u", hwifName.c_str(), swIfIndex);

    return true;
}

bool VppInterfaceRegistry::unbindSwIfIndex(
        _In_ const std::string& hwifName)
{
    SWSS_LOG_ENTER();

    auto rec = findByHwif(hwifName);

    if (!rec || !rec->hasSwIfIndex())
    {
        return false;
    }

    m_bySwIfIndex.erase(rec->getSwIfIndex());

    rec->m_swIfIndex = VppInterface::SWIF_INDEX_INVALID;

    return true;
}

bool VppInterfaceRegistry::setTapName(
        _In_ const std::string& hwifName,
        _In_ const std::string& tapName)
{
    SWSS_LOG_ENTER();

    auto rec = findByHwif(hwifName);

    if (!rec)
    {
        SWSS_LOG_ERROR("cannot set tap %s: %s is not registered", tapName.c_str(), hwifName.c_str());

        return false;
    }

    if (tapName.empty())
    {
        SWSS_LOG_ERROR("refusing to set an empty tap name on %s; use clearTapName()", hwifName.c_str());

        return false;
    }

    auto it = m_byTap.find(tapName);

    if (it != m_byTap.end() && it->second != rec)
    {
        /*
         * Host netdev names are recycled too: a port removed and re-added gets
         * the same name back. Detach the stale holder rather than letting two
         * records claim one netdev.
         */
        SWSS_LOG_WARN("tap %s was still held by %s, reassigning to %s",
                tapName.c_str(), it->second->getHwifName().c_str(), hwifName.c_str());

        it->second->m_tapName.clear();

        m_byTap.erase(it);
    }

    if (rec->hasTapName() && rec->getTapName() != tapName)
    {
        m_byTap.erase(rec->getTapName());
    }

    rec->m_tapName = tapName;

    m_byTap[tapName] = rec;

    SWSS_LOG_INFO("interface %s now has host tap %s", hwifName.c_str(), tapName.c_str());

    return true;
}

bool VppInterfaceRegistry::clearTapName(
        _In_ const std::string& hwifName)
{
    SWSS_LOG_ENTER();

    auto rec = findByHwif(hwifName);

    if (!rec || !rec->hasTapName())
    {
        return false;
    }

    SWSS_LOG_INFO("interface %s lost host tap %s", hwifName.c_str(), rec->getTapName().c_str());

    m_byTap.erase(rec->getTapName());

    rec->m_tapName.clear();

    return true;
}

bool VppInterfaceRegistry::setOid(
        _In_ const std::string& hwifName,
        _In_ sai_object_id_t oid)
{
    SWSS_LOG_ENTER();

    auto rec = findByHwif(hwifName);

    if (!rec)
    {
        SWSS_LOG_ERROR("cannot set oid on %s: not registered", hwifName.c_str());

        return false;
    }

    /*
     * by_oid is deliberately partial. Only a PORT or a LAG has the kind of oid
     * the FDB path resolves to; admitting ROUTER_INTERFACE or NEXT_HOP oids
     * here would destroy that contract.
     */
    if (rec->getType() != VppInterfaceType::PHYSICAL_PORT && rec->getType() != VppInterfaceType::LAG)
    {
        SWSS_LOG_ERROR("refusing to set port oid on %s of type %d; use setRifOid()",
                hwifName.c_str(), static_cast<int>(rec->getType()));

        return false;
    }

    if (rec->hasOid())
    {
        m_byOid.erase(rec->getOid());
    }

    rec->m_oid = oid;

    if (oid != SAI_NULL_OBJECT_ID)
    {
        m_byOid[oid] = rec;
    }

    return true;
}

bool VppInterfaceRegistry::setRifOid(
        _In_ const std::string& hwifName,
        _In_ sai_object_id_t rifOid)
{
    SWSS_LOG_ENTER();

    auto rec = findByHwif(hwifName);

    if (!rec)
    {
        SWSS_LOG_ERROR("cannot set rif oid on %s: not registered", hwifName.c_str());

        return false;
    }

    /* not indexed: nothing looks an interface up by its router interface oid yet */
    rec->m_rifOid = rifOid;

    return true;
}

bool VppInterfaceRegistry::setBdId(
        _In_ const std::string& hwifName,
        _In_ uint32_t bdId)
{
    SWSS_LOG_ENTER();

    auto rec = findByHwif(hwifName);

    if (!rec)
    {
        SWSS_LOG_ERROR("cannot set bd_id on %s: not registered", hwifName.c_str());

        return false;
    }

    rec->m_bdId = bdId;

    return true;
}

bool VppInterfaceRegistry::clearBdId(
        _In_ const std::string& hwifName)
{
    SWSS_LOG_ENTER();

    auto rec = findByHwif(hwifName);

    if (!rec)
    {
        return false;
    }

    rec->m_bdId = VppInterface::BD_ID_INVALID;

    return true;
}

size_t VppInterfaceRegistry::remove(
        _In_ const std::string& hwifName)
{
    SWSS_LOG_ENTER();

    auto rec = findByHwif(hwifName);

    if (!rec)
    {
        return 0;
    }

    size_t removed = 0;

    /*
     * Children must be collected before the parent is unindexed: dropping the
     * last owning reference would expire their weak parent pointer and lose the
     * link needed to find them.
     */
    for (auto& childName: collectChildren(rec))
    {
        removed += remove(childName);
    }

    unindexRecord(rec);

    removed++;

    SWSS_LOG_INFO("removed interface %s (%zu records)", hwifName.c_str(), removed);

    return removed;
}

size_t VppInterfaceRegistry::removeByOid(
        _In_ sai_object_id_t oid)
{
    SWSS_LOG_ENTER();

    auto it = m_byOid.find(oid);

    if (it == m_byOid.end())
    {
        return 0;
    }

    return remove(it->second->getHwifName());
}

std::shared_ptr<VppInterface> VppInterfaceRegistry::findByHwif(
        _In_ const std::string& hwifName) const
{
    auto it = m_byHwif.find(hwifName);

    return it == m_byHwif.end() ? nullptr : it->second;
}

std::shared_ptr<VppInterface> VppInterfaceRegistry::findBySonicName(
        _In_ const std::string& sonicName) const
{
    auto it = m_bySonicName.find(sonicName);

    return it == m_bySonicName.end() ? nullptr : it->second;
}

std::shared_ptr<VppInterface> VppInterfaceRegistry::findByTap(
        _In_ const std::string& tapName) const
{
    auto it = m_byTap.find(tapName);

    return it == m_byTap.end() ? nullptr : it->second;
}

std::shared_ptr<VppInterface> VppInterfaceRegistry::findByOid(
        _In_ sai_object_id_t oid) const
{
    auto it = m_byOid.find(oid);

    return it == m_byOid.end() ? nullptr : it->second;
}

std::shared_ptr<VppInterface> VppInterfaceRegistry::findBySwIfIndex(
        _In_ uint32_t swIfIndex) const
{
    auto it = m_bySwIfIndex.find(swIfIndex);

    return it == m_bySwIfIndex.end() ? nullptr : it->second;
}

std::shared_ptr<VppSubInterface> VppInterfaceRegistry::findSubIf(
        _In_ const std::string& parentHwifName,
        _In_ uint32_t subId) const
{
    /*
     * A sub-interface is named after its parent, so the primary index already
     * answers this; no separate (parent, vlan) index has to be kept in sync.
     */
    auto rec = findByHwif(VppSubInterface::hwifNameFor(parentHwifName, subId));

    if (!rec)
    {
        return nullptr;
    }

    return std::dynamic_pointer_cast<VppSubInterface>(rec);
}

sai_object_id_t VppInterfaceRegistry::resolvePortOid(
        _In_ uint32_t swIfIndex) const
{
    SWSS_LOG_ENTER();

    auto rec = findBySwIfIndex(swIfIndex);

    if (!rec)
    {
        return SAI_NULL_OBJECT_ID;
    }

    if (rec->getType() == VppInterfaceType::SUB_INTERFACE)
    {
        /*
         * VPP reports the sub-interface index in an FDB learn event, but SAI
         * knows the MAC on the parent PORT or LAG, which is what
         * SAI_BRIDGE_PORT_ATTR_PORT_ID holds.
         */
        auto sub = rec->asSubIf();

        auto parent = sub->getParent();

        if (!parent)
        {
            SWSS_LOG_ERROR("sub-interface %s has an expired parent", rec->getHwifName().c_str());

            return SAI_NULL_OBJECT_ID;
        }

        return parent->getOid();
    }

    return rec->getOid();
}

void VppInterfaceRegistry::clear()
{
    SWSS_LOG_ENTER();

    m_bySwIfIndex.clear();
    m_byOid.clear();
    m_byTap.clear();
    m_bySonicName.clear();
    m_byHwif.clear();
}

void VppInterfaceRegistry::indexRecord(
        _In_ const std::shared_ptr<VppInterface>& rec)
{
    m_byHwif[rec->getHwifName()] = rec;

    if (rec->hasSonicName())
    {
        m_bySonicName[rec->getSonicName()] = rec;
    }

    if (rec->hasTapName())
    {
        m_byTap[rec->getTapName()] = rec;
    }

    if (rec->hasOid())
    {
        m_byOid[rec->getOid()] = rec;
    }

    if (rec->hasSwIfIndex())
    {
        m_bySwIfIndex[rec->getSwIfIndex()] = rec;
    }
}

void VppInterfaceRegistry::unindexRecord(
        _In_ const std::shared_ptr<VppInterface>& rec)
{
    if (rec->hasSwIfIndex())
    {
        m_bySwIfIndex.erase(rec->getSwIfIndex());
    }

    if (rec->hasOid())
    {
        m_byOid.erase(rec->getOid());
    }

    if (rec->hasTapName())
    {
        m_byTap.erase(rec->getTapName());
    }

    if (rec->hasSonicName())
    {
        m_bySonicName.erase(rec->getSonicName());
    }

    m_byHwif.erase(rec->getHwifName());
}

std::vector<std::string> VppInterfaceRegistry::collectChildren(
        _In_ const std::shared_ptr<VppInterface>& parent) const
{
    std::vector<std::string> children;

    /*
     * Linear scan rather than a parent->children index. Removal is rare and the
     * table is small, and the alternative is a fifth index that would have to be
     * kept consistent on every mutation.
     */
    for (auto& kv: m_byHwif)
    {
        auto sub = kv.second->asSubIf();

        if (sub && sub->getParent() == parent)
        {
            children.push_back(kv.first);
        }
    }

    return children;
}
