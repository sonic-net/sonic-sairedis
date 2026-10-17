/*
 * NAT on VPP's nat44-ed plugin.
 *
 * SAI describes NAT as stateless table entries plus a zone id per router
 * interface: a packet whose ingress and egress router interfaces are in
 * different zones is looked up in the NAT table. nat44-ed has no zones; it
 * has inside and outside interfaces. The mapping used here:
 *
 *  - Zone 0 is the default zone and gets no NAT feature. A router interface
 *    with a non-zero zone becomes a nat44-ed outside interface in
 *    output-feature mode, so a packet leaving it (from any other interface)
 *    is source-translated by in2out-output, and a packet arriving on it is
 *    destination-translated by out2in. Traffic between two zone 0
 *    interfaces never reaches the NAT nodes.
 *
 *  - A SOURCE_NAT entry (src X -> Y) and a DESTINATION_NAT entry
 *    (dst Y -> X) are the two halves of one nat44-ed static mapping with
 *    local X and external Y; the mapping lives as long as either entry does.
 *    With L4 ports in the key the mapping is a static NAPT for that protocol
 *    and port pair. DESTINATION_NAT_POOL needs no VPP state: out2in already
 *    owns the external address of every mapping.
 *
 *  - SAI_SWITCH_ATTR_NAT_ENABLE enables the plugin (and nat44 forwarding, so
 *    traffic arriving on an outside interface that matches no mapping, such
 *    as routing protocols or traffic for the switch itself, is forwarded
 *    untranslated instead of dropped). Disabling it removes everything.
 *
 * Per-entry packet and byte counters are summed from the sessions nat44-ed
 * keeps for the mapping's local address, kept monotonic across session
 * expiry.
 *
 * Not supported (accepted and stored, nothing programmed): DOUBLE_NAT,
 * entries that are not /32, more than one distinct non-zero zone (all
 * non-zero zones are the same outside), translation when the pre-NAT
 * source sits behind a non-zero zone (nat44-ed only source-translates
 * towards an outside interface), dynamic NAT (SNAT_MISS/DNAT_MISS do not
 * reach the kernel; a flow towards an outside interface that matches no
 * mapping is dropped by nat44-ed).
 */

#include "SwitchVpp.h"
#include "SwitchVppUtils.h"

#include "meta/sai_serialize.h"
#include "swss/logger.h"

#include <arpa/inet.h>

using namespace saivs;

namespace
{
    std::string ip4Str(
            uint32_t addr)
    {
        SWSS_LOG_ENTER();

        char buf[INET_ADDRSTRLEN];
        struct in_addr a;

        a.s_addr = addr;
        inet_ntop(AF_INET, &a, buf, sizeof(buf));

        return buf;
    }

    const sai_attribute_t* findAttr(
            sai_attr_id_t id,
            uint32_t attr_count,
            const sai_attribute_t *attr_list)
    {
        SWSS_LOG_ENTER();

        for (uint32_t i = 0; i < attr_count; i++)
        {
            if (attr_list[i].id == id)
            {
                return &attr_list[i];
            }
        }

        return nullptr;
    }

    struct SessionDumpCtx
    {
        uint32_t external;
        uint16_t external_port;
        bool addr_only;
        std::map<std::tuple<uint32_t, uint16_t, uint32_t, uint16_t, uint16_t>, std::pair<uint64_t, uint64_t>> sessions;
    };

    void collectSession(
            const vpp_nat44_session_t *s,
            void *ctx)
    {
        SWSS_LOG_ENTER();

        auto *dctx = static_cast<SessionDumpCtx*>(ctx);

        // only sessions translated by this mapping
        if (s->outside_addr.s_addr != dctx->external)
        {
            return;
        }

        if (!dctx->addr_only && s->outside_port != dctx->external_port)
        {
            return;
        }

        auto key = std::make_tuple((uint32_t)s->inside_addr.s_addr, s->inside_port,
                (uint32_t)s->ext_host_addr.s_addr, s->ext_host_port, s->protocol);

        dctx->sessions[key] = { s->total_pkts, s->total_bytes };
    }
}

bool SwitchVpp::natSupported() const
{
    SWSS_LOG_ENTER();

    return vpp_nat44_ed_supported();
}

bool SwitchVpp::natResolveRifHwif(
        _In_ sai_object_id_t rif_id,
        _Out_ std::string &hwif_name)
{
    SWSS_LOG_ENTER();

    hwif_name.clear();

    sai_attribute_t attr;

    attr.id = SAI_ROUTER_INTERFACE_ATTR_TYPE;

    if (get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, rif_id, 1, &attr) != SAI_STATUS_SUCCESS)
    {
        return false;
    }

    int32_t rif_type = attr.value.s32;

    if (rif_type == SAI_ROUTER_INTERFACE_TYPE_VLAN)
    {
        // a VLAN router interface is the BVI "bvi<vlan id>"
        attr.id = SAI_ROUTER_INTERFACE_ATTR_VLAN_ID;

        if (get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, rif_id, 1, &attr) != SAI_STATUS_SUCCESS ||
            objectTypeQuery(attr.value.oid) != SAI_OBJECT_TYPE_VLAN)
        {
            return false;
        }

        sai_object_id_t vlan_oid = attr.value.oid;

        attr.id = SAI_VLAN_ATTR_VLAN_ID;

        if (get(SAI_OBJECT_TYPE_VLAN, vlan_oid, 1, &attr) != SAI_STATUS_SUCCESS || attr.value.u16 == 0)
        {
            return false;
        }

        hwif_name = std::string("bvi") + std::to_string(attr.value.u16);

        return true;
    }

    if (rif_type != SAI_ROUTER_INTERFACE_TYPE_PORT && rif_type != SAI_ROUTER_INTERFACE_TYPE_SUB_PORT)
    {
        return false;
    }

    uint16_t vlan_id = 0;

    if (rif_type == SAI_ROUTER_INTERFACE_TYPE_SUB_PORT)
    {
        attr.id = SAI_ROUTER_INTERFACE_ATTR_OUTER_VLAN_ID;

        if (get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, rif_id, 1, &attr) == SAI_STATUS_SUCCESS)
        {
            vlan_id = attr.value.u16;
        }
    }

    attr.id = SAI_ROUTER_INTERFACE_ATTR_PORT_ID;

    if (get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, rif_id, 1, &attr) != SAI_STATUS_SUCCESS)
    {
        return false;
    }

    hwif_name = m_ifaceRegistry.resolveHwIfName(attr.value.oid, vlan_id);

    return !hwif_name.empty();
}

int SwitchVpp::natProgramOutside(
        _In_ const std::string &hwif_name,
        _In_ bool is_add)
{
    SWSS_LOG_ENTER();

    int rc = vpp_nat44_ed_output_interface_add_del(hwif_name.c_str(), is_add);

    if (rc != 0)
    {
        SWSS_LOG_ERROR("nat44 outside %s on %s failed: %d", is_add ? "add" : "del", hwif_name.c_str(), rc);
    }
    else
    {
        SWSS_LOG_NOTICE("nat44 outside %s on %s", is_add ? "add" : "del", hwif_name.c_str());
    }

    return rc;
}

int SwitchVpp::natProgramMapping(
        _In_ const NatMapping &m,
        _In_ bool is_add)
{
    SWSS_LOG_ENTER();

    vpp_nat44_static_mapping_t vm;

    memset(&vm, 0, sizeof(vm));
    vm.local.s_addr = m.local;
    vm.external.s_addr = m.external;
    vm.protocol = m.protocol;
    vm.local_port = m.local_port;
    vm.external_port = m.external_port;
    vm.vrf_id = m.vrf_id;
    vm.addr_only = m.addr_only;

    int rc = vpp_nat44_static_mapping_add_del(&vm, is_add);

    if (rc != 0)
    {
        SWSS_LOG_ERROR("nat44 static mapping %s local %s:%u external %s:%u proto %u vrf %u failed: %d",
                is_add ? "add" : "del", ip4Str(m.local).c_str(), m.local_port,
                ip4Str(m.external).c_str(), m.external_port, m.protocol, m.vrf_id, rc);
    }
    else
    {
        SWSS_LOG_NOTICE("nat44 static mapping %s local %s:%u external %s:%u proto %u vrf %u",
                is_add ? "add" : "del", ip4Str(m.local).c_str(), m.local_port,
                ip4Str(m.external).c_str(), m.external_port, m.protocol, m.vrf_id);
    }

    return rc;
}

bool SwitchVpp::natEntryMapping(
        _In_ const sai_nat_entry_t &entry,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list,
        _Out_ NatMapping &m)
{
    SWSS_LOG_ENTER();

    memset(&m, 0, sizeof(m));

    auto vrf = vpp_get_ip_vrf(entry.vr_id);

    m.vrf_id = vrf ? vrf->m_vrf_id : 0;

    const auto &key = entry.data.key;
    const auto &mask = entry.data.mask;

    bool has_proto = mask.proto != 0;

    switch (entry.nat_type)
    {
        case SAI_NAT_TYPE_SOURCE_NAT:
        {
            auto ip = findAttr(SAI_NAT_ENTRY_ATTR_SRC_IP, attr_count, attr_list);
            auto ipmask = findAttr(SAI_NAT_ENTRY_ATTR_SRC_IP_MASK, attr_count, attr_list);
            auto port = findAttr(SAI_NAT_ENTRY_ATTR_L4_SRC_PORT, attr_count, attr_list);

            if (mask.src_ip != 0xffffffff || mask.dst_ip != 0 || mask.l4_dst_port != 0 || !ip ||
                (ipmask && ipmask->value.ip4 != 0xffffffff))
            {
                SWSS_LOG_NOTICE("SOURCE_NAT entry not supported on VPP (needs a /32 source key and SRC_IP)");
                return false;
            }

            m.local = key.src_ip;
            m.external = ip->value.ip4;

            if (has_proto || mask.l4_src_port != 0)
            {
                m.protocol = key.proto;
                m.local_port = key.l4_src_port;
                m.external_port = port ? port->value.u16 : key.l4_src_port;
            }
            else
            {
                m.addr_only = true;
            }

            return true;
        }

        case SAI_NAT_TYPE_DESTINATION_NAT:
        {
            auto ip = findAttr(SAI_NAT_ENTRY_ATTR_DST_IP, attr_count, attr_list);
            auto ipmask = findAttr(SAI_NAT_ENTRY_ATTR_DST_IP_MASK, attr_count, attr_list);
            auto port = findAttr(SAI_NAT_ENTRY_ATTR_L4_DST_PORT, attr_count, attr_list);

            if (mask.dst_ip != 0xffffffff || mask.src_ip != 0 || mask.l4_src_port != 0 || !ip ||
                (ipmask && ipmask->value.ip4 != 0xffffffff))
            {
                SWSS_LOG_NOTICE("DESTINATION_NAT entry not supported on VPP (needs a /32 destination key and DST_IP)");
                return false;
            }

            m.local = ip->value.ip4;
            m.external = key.dst_ip;

            if (has_proto || mask.l4_dst_port != 0)
            {
                m.protocol = key.proto;
                m.external_port = key.l4_dst_port;
                m.local_port = port ? port->value.u16 : key.l4_dst_port;
            }
            else
            {
                m.addr_only = true;
            }

            return true;
        }

        case SAI_NAT_TYPE_DESTINATION_NAT_POOL:
            // out2in owns the external address of every static mapping
            return false;

        default:
            SWSS_LOG_NOTICE("NAT entry type %d not supported on VPP, stored only", entry.nat_type);
            return false;
    }
}

void SwitchVpp::natMappingRef(
        _In_ const std::string &entry_id,
        _In_ const NatMapping &m)
{
    SWSS_LOG_ENTER();

    auto &owners = m_nat_mappings[m];

    owners.insert(entry_id);

    if (owners.size() == 1 && m_nat_vpp_enabled)
    {
        natProgramMapping(m, true);
    }
}

void SwitchVpp::natMappingUnref(
        _In_ const std::string &entry_id)
{
    SWSS_LOG_ENTER();

    auto it = m_nat_entries.find(entry_id);

    if (it == m_nat_entries.end() || !it->second.has_mapping)
    {
        return;
    }

    const NatMapping m = it->second.mapping;

    it->second.has_mapping = false;

    auto mit = m_nat_mappings.find(m);

    if (mit == m_nat_mappings.end())
    {
        return;
    }

    mit->second.erase(entry_id);

    if (!mit->second.empty())
    {
        return;
    }

    if (m_nat_vpp_enabled)
    {
        natProgramMapping(m, false);
    }

    m_nat_mappings.erase(mit);
    m_nat_counters.erase(m);
}

sai_status_t SwitchVpp::createNatEntry(
        _In_ const std::string &serializedObjectId,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    CHECK_STATUS(create_internal(SAI_OBJECT_TYPE_NAT_ENTRY, serializedObjectId, switch_id, attr_count, attr_list));

    sai_nat_entry_t entry;

    sai_deserialize_nat_entry(serializedObjectId, entry);

    NatEntryState state;

    state.has_mapping = natEntryMapping(entry, attr_count, attr_list, state.mapping);

    m_nat_entries[serializedObjectId] = state;

    if (state.has_mapping)
    {
        natMappingRef(serializedObjectId, state.mapping);
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::removeNatEntry(
        _In_ const std::string &serializedObjectId)
{
    SWSS_LOG_ENTER();

    natMappingUnref(serializedObjectId);

    m_nat_entries.erase(serializedObjectId);

    return remove_internal(SAI_OBJECT_TYPE_NAT_ENTRY, serializedObjectId);
}

void SwitchVpp::natReadCounters(
        _In_ const NatMapping &m,
        _Out_ uint64_t &pkts,
        _Out_ uint64_t &bytes)
{
    SWSS_LOG_ENTER();

    auto &c = m_nat_counters[m];

    pkts = c.base_pkts;
    bytes = c.base_bytes;

    if (!m_nat_vpp_enabled)
    {
        for (auto &l: c.live)
        {
            pkts += l.second.first;
            bytes += l.second.second;
        }

        return;
    }

    SessionDumpCtx dctx;

    dctx.external = m.external;
    dctx.external_port = m.external_port;
    dctx.addr_only = m.addr_only;

    struct in_addr local;

    local.s_addr = m.local;

    if (vpp_nat44_user_sessions_dump(local, m.vrf_id, collectSession, &dctx) != 0)
    {
        // keep what was last seen
        for (auto &l: c.live)
        {
            pkts += l.second.first;
            bytes += l.second.second;
        }

        return;
    }

    std::map<NatSessionKey, std::pair<uint64_t, uint64_t>> live;

    for (auto &s: dctx.sessions)
    {
        NatSessionKey k = { std::get<0>(s.first), std::get<1>(s.first), std::get<2>(s.first),
            std::get<3>(s.first), std::get<4>(s.first) };

        live[k] = s.second;
    }

    for (auto &prev: c.live)
    {
        auto cur = live.find(prev.first);

        // a session that expired, or expired and came back with fresh totals
        if (cur == live.end() || cur->second.first < prev.second.first)
        {
            c.base_pkts += prev.second.first;
            c.base_bytes += prev.second.second;
        }
    }

    c.live.swap(live);

    pkts = c.base_pkts;
    bytes = c.base_bytes;

    for (auto &l: c.live)
    {
        pkts += l.second.first;
        bytes += l.second.second;
    }
}

sai_status_t SwitchVpp::getNatEntry(
        _In_ const std::string &serializedObjectId,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    const auto &objectHash = m_objectHash.at(SAI_OBJECT_TYPE_NAT_ENTRY);

    auto oit = objectHash.find(serializedObjectId);

    if (oit == objectHash.end())
    {
        return SAI_STATUS_ITEM_NOT_FOUND;
    }

    auto &state = m_nat_entries[serializedObjectId];

    bool read = false;
    bool clear_hit = false;
    uint64_t pkts = 0, bytes = 0;

    auto readOnce = [&]() {
        if (!read && state.has_mapping)
        {
            natReadCounters(state.mapping, pkts, bytes);
        }

        read = true;
    };

    for (uint32_t idx = 0; idx < attr_count; idx++)
    {
        switch (attr_list[idx].id)
        {
            case SAI_NAT_ENTRY_ATTR_PACKET_COUNT:
                readOnce();
                attr_list[idx].value.u64 = pkts >= state.zero_pkts ? pkts - state.zero_pkts : 0;
                continue;

            case SAI_NAT_ENTRY_ATTR_BYTE_COUNT:
                readOnce();
                attr_list[idx].value.u64 = bytes >= state.zero_bytes ? bytes - state.zero_bytes : 0;
                continue;

            case SAI_NAT_ENTRY_ATTR_HIT_BIT:
            case SAI_NAT_ENTRY_ATTR_HIT_BIT_COR:
                readOnce();
                attr_list[idx].value.booldata = pkts > state.hit_mark_pkts;
                clear_hit |= (attr_list[idx].id == SAI_NAT_ENTRY_ATTR_HIT_BIT_COR);
                continue;

            default:
                break;
        }

        auto meta = sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_NAT_ENTRY, attr_list[idx].id);

        if (meta == NULL)
        {
            return SAI_STATUS_FAILURE;
        }

        auto ait = oit->second.find(meta->attridname);

        if (ait == oit->second.end())
        {
            return SAI_STATUS_ITEM_NOT_FOUND;
        }

        CHECK_STATUS(transfer_attributes(SAI_OBJECT_TYPE_NAT_ENTRY, 1, ait->second->getAttr(), &attr_list[idx], false));
    }

    if (clear_hit)
    {
        state.hit_mark_pkts = pkts;
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::setNatEntry(
        _In_ const std::string &serializedObjectId,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    CHECK_STATUS(set_internal(SAI_OBJECT_TYPE_NAT_ENTRY, serializedObjectId, attr));

    auto &state = m_nat_entries[serializedObjectId];

    switch (attr->id)
    {
        case SAI_NAT_ENTRY_ATTR_PACKET_COUNT:
        case SAI_NAT_ENTRY_ATTR_BYTE_COUNT:
        {
            uint64_t pkts = 0, bytes = 0;

            if (state.has_mapping)
            {
                natReadCounters(state.mapping, pkts, bytes);
            }

            // the counter reads attr->value.u64 from now on (orchagent clears with 0)
            if (attr->id == SAI_NAT_ENTRY_ATTR_PACKET_COUNT)
            {
                state.zero_pkts = pkts >= attr->value.u64 ? pkts - attr->value.u64 : 0;
            }
            else
            {
                state.zero_bytes = bytes >= attr->value.u64 ? bytes - attr->value.u64 : 0;
            }

            return SAI_STATUS_SUCCESS;
        }

        case SAI_NAT_ENTRY_ATTR_SRC_IP:
        case SAI_NAT_ENTRY_ATTR_SRC_IP_MASK:
        case SAI_NAT_ENTRY_ATTR_DST_IP:
        case SAI_NAT_ENTRY_ATTR_DST_IP_MASK:
        case SAI_NAT_ENTRY_ATTR_L4_SRC_PORT:
        case SAI_NAT_ENTRY_ATTR_L4_DST_PORT:
        case SAI_NAT_ENTRY_ATTR_VR_ID:
        {
            // the translation changed: rebuild the mapping from what is stored
            std::vector<sai_attribute_t> attrs;

            for (auto &a: m_objectHash.at(SAI_OBJECT_TYPE_NAT_ENTRY).at(serializedObjectId))
            {
                attrs.push_back(*a.second->getAttr());
            }

            sai_nat_entry_t entry;

            sai_deserialize_nat_entry(serializedObjectId, entry);

            NatMapping m;
            bool has = natEntryMapping(entry, (uint32_t)attrs.size(), attrs.data(), m);

            natMappingUnref(serializedObjectId);

            auto &st = m_nat_entries[serializedObjectId];

            st.has_mapping = has;

            if (has)
            {
                st.mapping = m;
                natMappingRef(serializedObjectId, m);
            }

            return SAI_STATUS_SUCCESS;
        }

        default:
            return SAI_STATUS_SUCCESS;
    }
}

sai_status_t SwitchVpp::natSetEnable(
        _In_ bool enable)
{
    SWSS_LOG_ENTER();

    m_nat_enabled = enable;

    if (enable == m_nat_vpp_enabled)
    {
        return SAI_STATUS_SUCCESS;
    }

    if (enable)
    {
        if (!natSupported())
        {
            SWSS_LOG_ERROR("NAT enabled but VPP does not run nat_plugin.so; NAT entries will not translate");
            return SAI_STATUS_SUCCESS;
        }

        int rc = vpp_nat44_ed_enable_disable(true, 0);

        if (rc != 0)
        {
            SWSS_LOG_ERROR("nat44-ed plugin enable failed: %d", rc);
            return SAI_STATUS_FAILURE;
        }

        if (vpp_nat44_forwarding_enable_disable(true) != 0)
        {
            SWSS_LOG_ERROR("nat44 forwarding enable failed; unmatched traffic into a NAT zone will be dropped");
        }

        m_nat_vpp_enabled = true;

        SWSS_LOG_NOTICE("nat44-ed enabled: %zu outside interfaces, %zu static mappings",
                m_nat_zone_rifs.size(), m_nat_mappings.size());

        for (auto &z: m_nat_zone_rifs)
        {
            natProgramOutside(z.second.second, true);
        }

        for (auto &m: m_nat_mappings)
        {
            natProgramMapping(m.first, true);
        }

        return SAI_STATUS_SUCCESS;
    }

    // disabling the plugin drops every mapping, interface and session in VPP
    for (auto &m: m_nat_mappings)
    {
        natProgramMapping(m.first, false);
    }

    for (auto &z: m_nat_zone_rifs)
    {
        natProgramOutside(z.second.second, false);
    }

    int rc = vpp_nat44_ed_enable_disable(false, 0);

    if (rc != 0)
    {
        SWSS_LOG_ERROR("nat44-ed plugin disable failed: %d", rc);
    }

    m_nat_vpp_enabled = false;

    for (auto &c: m_nat_counters)
    {
        // sessions are gone: their totals become history
        for (auto &l: c.second.live)
        {
            c.second.base_pkts += l.second.first;
            c.second.base_bytes += l.second.second;
        }

        c.second.live.clear();
    }

    SWSS_LOG_NOTICE("nat44-ed disabled");

    return SAI_STATUS_SUCCESS;
}

void SwitchVpp::natRouterInterfaceZone(
        _In_ sai_object_id_t rif_id,
        _In_ uint32_t zone_id)
{
    SWSS_LOG_ENTER();

    auto it = m_nat_zone_rifs.find(rif_id);

    if (zone_id == 0)
    {
        if (it != m_nat_zone_rifs.end())
        {
            if (m_nat_vpp_enabled)
            {
                natProgramOutside(it->second.second, false);
            }

            m_nat_zone_rifs.erase(it);
        }

        return;
    }

    std::string hwif;

    if (!natResolveRifHwif(rif_id, hwif))
    {
        SWSS_LOG_ERROR("NAT zone %u on router interface %s: no VPP interface, not programmed",
                zone_id, sai_serialize_object_id(rif_id).c_str());
        return;
    }

    if (it != m_nat_zone_rifs.end())
    {
        it->second.first = zone_id;

        if (it->second.second == hwif)
        {
            return;
        }

        if (m_nat_vpp_enabled)
        {
            natProgramOutside(it->second.second, false);
        }
    }

    for (auto &z: m_nat_zone_rifs)
    {
        if (z.first != rif_id && z.second.first != zone_id)
        {
            SWSS_LOG_NOTICE("NAT zones %u and %u are both nat44 outside on VPP; traffic between them is not translated",
                    z.second.first, zone_id);
            break;
        }
    }

    m_nat_zone_rifs[rif_id] = { zone_id, hwif };

    if (m_nat_vpp_enabled)
    {
        natProgramOutside(hwif, true);
    }
}

void SwitchVpp::natRouterInterfaceRemoved(
        _In_ sai_object_id_t rif_id)
{
    SWSS_LOG_ENTER();

    natRouterInterfaceZone(rif_id, 0);
}
