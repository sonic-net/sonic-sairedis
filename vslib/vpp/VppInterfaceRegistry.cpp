#include "VppInterfaceRegistry.h"

#include "swss/logger.h"

#include <cctype>
#include <cstdio>
#include <cstring>

using namespace saivs;

// TODO to config
static const char *sonic_vpp_ifmap = "/usr/share/sonic/hwsku/sonic_vpp_ifmap.ini";

/*
 * Split a trailing ".<vlan>" sub-interface suffix off an interface name.
 * Returns false if there is a '.' that is not followed by digits only, which
 * means the name is not something this switch ever creates.
 */
static bool split_subif_suffix(
        _In_ const std::string& name,
        _Out_ std::string& base,
        _Out_ std::string& suffix)
{
    SWSS_LOG_ENTER();

    auto dot = name.find('.');

    if (dot == std::string::npos)
    {
        base = name;
        suffix.clear();

        return true;
    }

    base = name.substr(0, dot);
    suffix = name.substr(dot);

    if (suffix.size() < 2)
    {
        return false;
    }

    for (size_t i = 1; i < suffix.size(); i++)
    {
        if (!isdigit(static_cast<unsigned char>(suffix[i])))
        {
            return false;
        }
    }

    return true;
}

/*
 * Derive the VPP hardware interface name of a logical interface. These are
 * hard invariants of the way this switch creates VPP interfaces:
 *
 *   PortChannel<N> -> BondEthernet<N>   vpp_create_lag(); find_new_bond_id()
 *                                       takes the bond id straight from the
 *                                       PortChannel kernel netdev name
 *   Vlan<N>        -> bvi<N>            vpp_create_bvi_interface()
 *
 * Only SONiC-side names are accepted. The bond's linux-cp tap name be<N> is
 * deliberately not resolved here: it is a VPP-side artefact, and accepting it
 * would let a caller that wants the SONiC netdev (`ip link show`, VRF
 * enslavement) silently get away with the wrong name.
 *
 * Physical ports are not handled here: their hwif name is platform specific
 * and only sonic_vpp_ifmap.ini knows it.
 *
 * The id is carried across as a string rather than parsed, so this cannot
 * disagree with the VppInterface name rules over what a large or leading-zero
 * id turns into.
 */
static bool derive_logical_hwif(
        _In_ const std::string& base,
        _Out_ std::string& hwif)
{
    SWSS_LOG_ENTER();

    // prefixes must match PORTCHANNEL_PREFIX / VLAN_PREFIX in SwitchVppUtils.h,
    // results must match VppBondInterface / VppVlanInterface hwifNameFor()
    const char *prefix;
    const char *hwif_prefix;

    if (base.compare(0, 11, "PortChannel") == 0)
    {
        prefix = "PortChannel";
        hwif_prefix = "BondEthernet";
    }
    else if (base.compare(0, 4, "Vlan") == 0)
    {
        prefix = "Vlan";
        hwif_prefix = "bvi";
    }
    else
    {
        return false;
    }

    std::string id = base.substr(strlen(prefix));

    if (id.empty())
    {
        return false;
    }

    for (size_t i = 0; i < id.size(); i++)
    {
        if (!isdigit(static_cast<unsigned char>(id[i])))
        {
            return false;
        }
    }

    hwif = std::string(hwif_prefix) + id;

    return true;
}

std::shared_ptr<VppPhysicalPort> VppInterfaceRegistry::addPhysicalPort(
        _In_ const std::string& hwifName,
        _In_ const std::string& osIf,
        _In_ sai_object_id_t oid)
{
    SWSS_LOG_ENTER();

    if (oid == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_ERROR("refusing to register port %s with a null oid", osIf.c_str());

        return nullptr;
    }

    auto existing = findByHwif(hwifName);

    if (existing)
    {
        SWSS_LOG_WARN("interface %s already registered as type %d, not adding physical port",
                hwifName.c_str(), static_cast<int>(existing->getType()));

        return nullptr;
    }

    auto rec = std::make_shared<VppPhysicalPort>(hwifName, osIf);

    indexRecord(rec);

    setOid(hwifName, oid);

    SWSS_LOG_INFO("registered physical port %s as hwif %s oid 0x%llx",
            osIf.c_str(), hwifName.c_str(), static_cast<unsigned long long>(oid));

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
        _In_ uint16_t vlanId,
        _In_ sai_object_id_t vlanOid)
{
    SWSS_LOG_ENTER();

    auto hwifName = VppVlanInterface::hwifNameFor(vlanId);

    if (findByHwif(hwifName))
    {
        SWSS_LOG_WARN("interface %s already registered, not adding BVI", hwifName.c_str());

        return nullptr;
    }

    if (vlanOid == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_ERROR("refusing to register BVI %s with a null vlan oid", hwifName.c_str());

        return nullptr;
    }

    auto rec = std::make_shared<VppVlanInterface>(vlanId);

    indexRecord(rec);

    setOid(hwifName, vlanOid);

    SWSS_LOG_INFO("registered BVI %s oid 0x%llx",
            hwifName.c_str(), static_cast<unsigned long long>(vlanOid));

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
     * by_oid is deliberately partial. It admits the three object kinds that
     * name a forwarding interface -- PORT, LAG and the VLAN behind a BVI --
     * and nothing else; admitting ROUTER_INTERFACE or NEXT_HOP oids here would
     * destroy that contract.
     */
    if (rec->getType() != VppInterfaceType::PHYSICAL_PORT &&
            rec->getType() != VppInterfaceType::LAG &&
            rec->getType() != VppInterfaceType::VLAN_BVI)
    {
        SWSS_LOG_ERROR("refusing to set oid on %s of type %d; use setRifOid()",
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
    SWSS_LOG_ENTER();

    auto it = m_byHwif.find(hwifName);

    return it == m_byHwif.end() ? nullptr : it->second;
}

std::shared_ptr<VppInterface> VppInterfaceRegistry::findByOsIf(
        _In_ const std::string& osIf) const
{
    SWSS_LOG_ENTER();

    auto it = m_byOsIf.find(osIf);

    return it == m_byOsIf.end() ? nullptr : it->second;
}

std::shared_ptr<VppInterface> VppInterfaceRegistry::findByTap(
        _In_ const std::string& tapName) const
{
    SWSS_LOG_ENTER();

    auto it = m_byTap.find(tapName);

    return it == m_byTap.end() ? nullptr : it->second;
}

std::shared_ptr<VppInterface> VppInterfaceRegistry::findByOid(
        _In_ sai_object_id_t oid) const
{
    SWSS_LOG_ENTER();

    auto it = m_byOid.find(oid);

    return it == m_byOid.end() ? nullptr : it->second;
}

std::shared_ptr<VppInterface> VppInterfaceRegistry::findBySwIfIndex(
        _In_ uint32_t swIfIndex) const
{
    SWSS_LOG_ENTER();

    auto it = m_bySwIfIndex.find(swIfIndex);

    return it == m_bySwIfIndex.end() ? nullptr : it->second;
}

std::shared_ptr<VppSubInterface> VppInterfaceRegistry::findSubIf(
        _In_ const std::string& parentHwifName,
        _In_ uint32_t subId) const
{
    SWSS_LOG_ENTER();

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

sai_object_id_t VppInterfaceRegistry::resolveIfOid(
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

        rec = parent;
    }

    return rec->getOid();
}

std::string VppInterfaceRegistry::resolveTapName(
        _In_ sai_object_id_t oid) const
{
    SWSS_LOG_ENTER();

    auto rec = findByOid(oid);

    if (rec && rec->hasTapName())
    {
        return rec->getTapName();
    }

    return std::string();
}

std::string VppInterfaceRegistry::resolveOsIf(
        _In_ sai_object_id_t oid) const
{
    SWSS_LOG_ENTER();

    auto rec = findByOid(oid);

    if (rec && rec->hasOsIf())
    {
        return rec->getOsIf();
    }

    return std::string();
}

std::string VppInterfaceRegistry::resolveOsIfByHwif(
        _In_ const std::string& hwifName) const
{
    SWSS_LOG_ENTER();

    auto rec = findByHwif(hwifName);

    if (rec && rec->hasOsIf())
    {
        return rec->getOsIf();
    }

    return std::string();
}

std::string VppInterfaceRegistry::resolveHwIfName(
        _In_ sai_object_id_t oid,
        _In_ uint32_t vlanId) const
{
    SWSS_LOG_ENTER();

    auto rec = findByOid(oid);

    if (!rec)
    {
        return std::string();
    }

    if (vlanId)
    {
        return rec->getHwifName() + "." + std::to_string(vlanId);
    }

    return rec->getHwifName();
}

/*
 * sonic_vpp_ifmap.ini maps the host OS interface name ("osif") to the VPP
 * hardware interface name, e.g. "Ethernet0" -> "TenGigabitEthernet0/0/0". The
 * osif name is whatever the NOS above SAI calls the port -- under SONiC it is
 * the port_config.ini name, but nothing here depends on that.
 * Only physical ports appear in the file -- every logical interface is derived
 * by rule in derive_logical_hwif() above.
 *
 * Note that for a physical port the osif name, the LCP tap name and the SAI
 * hostif name are all the same string, which is why this map has historically
 * been described as a "tap" map. It is not: for a bond the osif name is
 * "PortChannel<N>" while the tap is "be<N>", and the two must not be confused.
 */
void VppInterfaceRegistry::loadIfMapping() const
{
    SWSS_LOG_ENTER();

    if (m_ifMapLoaded)
    {
        return;
    }

    FILE *fp = fopen(sonic_vpp_ifmap, "r");

    if (!fp)
    {
        return;
    }

    char osif_name[64], vpp_name[64];

    while (fscanf(fp, "%63s %63s", osif_name, vpp_name) != EOF)
    {
        m_osIfToHwIf[std::string(osif_name)] = std::string(vpp_name);
    }

    m_ifMapLoaded = true;

    fclose(fp);
}

std::string VppInterfaceRegistry::resolveHwIfByOsIf(
        _In_ const std::string& osIfName) const
{
    SWSS_LOG_ENTER();

    loadIfMapping();

    auto it = m_osIfToHwIf.find(osIfName);

    if (it != m_osIfToHwIf.end())
    {
        return it->second;
    }

    std::string base, suffix;

    if (split_subif_suffix(osIfName, base, suffix))
    {
        /*
         * A sub-interface carries the ".<vlan>" suffix over unchanged, so
         * "Ethernet0.100" -> "TenGigabitEthernet0/0/0.100" and
         * "PortChannel1.100" -> "BondEthernet1.100".
         */
        auto baseIt = m_osIfToHwIf.find(base);

        if (baseIt != m_osIfToHwIf.end())
        {
            return baseIt->second + suffix;
        }

        std::string derived;

        if (derive_logical_hwif(base, derived))
        {
            return derived + suffix;
        }
    }

    SWSS_LOG_ERROR("failed to find hwif info entry for interface: %s", osIfName.c_str());

    return std::string();
}

std::shared_ptr<VppInterface> VppInterfaceRegistry::findLag(
        _In_ sai_object_id_t oid) const
{
    SWSS_LOG_ENTER();

    auto rec = findByOid(oid);

    if (rec && rec->getType() == VppInterfaceType::LAG && rec->hasSwIfIndex())
    {
        return rec;
    }

    return nullptr;
}

std::unordered_set<uint32_t> VppInterfaceRegistry::collectBondIds() const
{
    SWSS_LOG_ENTER();

    std::unordered_set<uint32_t> ids;

    for (const auto& kvp: m_byHwif)
    {
        const VppBondInterface *bond = kvp.second->asBond();

        if (bond)
        {
            ids.insert(bond->getBondId());
        }
    }

    return ids;
}

void VppInterfaceRegistry::clear()
{
    SWSS_LOG_ENTER();

    m_bySwIfIndex.clear();
    m_byOid.clear();
    m_byTap.clear();
    m_byOsIf.clear();
    m_byHwif.clear();
}

void VppInterfaceRegistry::indexRecord(
        _In_ const std::shared_ptr<VppInterface>& rec)
{
    SWSS_LOG_ENTER();

    m_byHwif[rec->getHwifName()] = rec;

    if (rec->hasOsIf())
    {
        m_byOsIf[rec->getOsIf()] = rec;
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
    SWSS_LOG_ENTER();

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

    if (rec->hasOsIf())
    {
        m_byOsIf.erase(rec->getOsIf());
    }

    m_byHwif.erase(rec->getHwifName());
}

std::vector<std::string> VppInterfaceRegistry::collectChildren(
        _In_ const std::shared_ptr<VppInterface>& parent) const
{
    SWSS_LOG_ENTER();

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
