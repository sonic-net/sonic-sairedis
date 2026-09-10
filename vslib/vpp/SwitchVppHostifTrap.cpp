/*
 * CoPP: SAI_OBJECT_TYPE_HOSTIF_TRAP_GROUP / SAI_OBJECT_TYPE_HOSTIF_TRAP
 * bookkeeping and VPP classify/punt binding.
 */

#include "SwitchVpp.h"
#include "SwitchVppUtils.h"
#include "SwitchVppHostifTrap.h"

#include "meta/sai_serialize.h"

#include "swss/logger.h"
#include "swss/exec.h"

#include <sstream>
#include <cstdio>
#include <cstring>
#include <array>

using namespace saivs;

namespace
{
    // Builds the 16-byte (mask_n_vectors=1 in VPP's u32x4 classify unit)
    // hex match key for a given trap_type's ethertype, matched at byte
    // offset 12-13 of the Ethernet frame (dst_mac[6]+src_mac[6] then
    // ethertype/length). Returns false if this trap_type has no
    // match-key mapping yet (caller should fall back to the logged
    // no-op).
    bool buildClassifyMatchForTrapType(
            _In_ sai_hostif_trap_type_t trap_type,
            _Out_ std::array<uint8_t, 16> &match)
    {
        match.fill(0);

        uint16_t ethertype;

        switch (trap_type)
        {
            case SAI_HOSTIF_TRAP_TYPE_ARP_REQUEST:
            case SAI_HOSTIF_TRAP_TYPE_ARP_RESPONSE:
                ethertype = 0x0806;
                break;

            case SAI_HOSTIF_TRAP_TYPE_LACP:
                ethertype = 0x8809;
                break;

            case SAI_HOSTIF_TRAP_TYPE_LLDP:
                ethertype = 0x88cc;
                break;

            case SAI_HOSTIF_TRAP_TYPE_UDLD:
                ethertype = 0x0067;
                break;

            case SAI_HOSTIF_TRAP_TYPE_TTL_ERROR:
                // DefaultTest sends a plain TCP/IP packet with ip_ttl=1
                // over ethertype 0x0800 (IPv4)
                ethertype = 0x0800;
                break;

            case SAI_HOSTIF_TRAP_TYPE_BGPV6:
            case SAI_HOSTIF_TRAP_TYPE_IPV6_NEIGHBOR_DISCOVERY:
            case SAI_HOSTIF_TRAP_TYPE_IPV6_NEIGHBOR_SOLICITATION:
            case SAI_HOSTIF_TRAP_TYPE_IPV6_NEIGHBOR_ADVERTISEMENT:
                // Prior to this case, no trap_type ever mapped to ethertype
                // 0x86DD (IPv6), so any IPv6 control packet (BGP-over-IPv6,
                // ND solicit/advert) fell through this node unmatched and
                // continued into the normal ethernet-input/ip6-input path.
                // On a LAG member that is harmless for a directly-attached
                // port, but LAG members intentionally run with IPv6 disabled
                // (see vpp_set_lag_member_ip6() in SwitchVppFdb.cpp -- L3 for
                // a bonded interface is owned by the BondEthernet, not the
                // member), so an unmatched IPv6 packet arriving on a member
                // hits ip6-not-enabled and is dropped before VPP's bond layer
                // ever gets a chance to reclassify it to the bond's
                // sw_if_index. Binding 0x86DD here routes it through the same
                // direct-to-tap path IPv4 already uses (matched entry ->
                // lcp_itf_pair_find_by_phy -> interface-output), bypassing
                // ip6-input entirely, exactly like the IPv4/TTL_ERROR path
                // above already does for IPv4.
                ethertype = 0x86dd;
                break;

            default:
                return false;
        }

        match[12] = (uint8_t)(ethertype >> 8);
        match[13] = (uint8_t)(ethertype & 0xFF);

        return true;
    }

    bool isIp4TtlExpiringTrap(sai_hostif_trap_type_t trap_type)
    {
        return trap_type == SAI_HOSTIF_TRAP_TYPE_TTL_ERROR;
    }

    bool isIp6BgpTrap(sai_hostif_trap_type_t trap_type)
    {
        return trap_type == SAI_HOSTIF_TRAP_TYPE_BGPV6;
    }

    bool isIp6NdTrap(sai_hostif_trap_type_t trap_type)
    {
        return trap_type == SAI_HOSTIF_TRAP_TYPE_IPV6_NEIGHBOR_DISCOVERY ||
               trap_type == SAI_HOSTIF_TRAP_TYPE_IPV6_NEIGHBOR_SOLICITATION ||
               trap_type == SAI_HOSTIF_TRAP_TYPE_IPV6_NEIGHBOR_ADVERTISEMENT;
    }
}


/*
 * Install (or update) the VPP-native punt-path policer binding for `trap`
 * across every physical (hwif) interface backing a currently-known hostif
 * TAP (SAI traps are switch-wide, not per-port, so this applies uniformly
 * to all ports SwitchVpp knows about via m_hostif_info_map -- see
 * SwitchVppHostif.cpp's vs_create_hostif_tap_interface()).
 */
sai_status_t SwitchVpp::installTrapClassify(
        _In_ sai_object_id_t trap_oid,
        _In_ const vpp_trap_entry_t &trap,
        _In_ uint32_t vpp_policer_index)
{
    SWSS_LOG_ENTER();

    // Deferred (WD-timeout fix, generalized -- see enqueueTrapClassifyDeferredWork()
    // in SwitchVpp.h): do NOT call vpp_copp_punt_policer_bind() synchronously
    // here -- it is a blocking VAPI round-trip and this function is called
    // directly from createHostifTrap()/setHostifTrap()/setHostifTrapGroup(),
    // i.e. from inside a single watchdog-timed syncd SAI call. Confirmed live:
    // a BGPV6 createHostifTrap call stalled past the 30s watchdog here,
    // starving every trap queued after it for the rest of that config_reload.
    // The real bind runs later, one item per call, from
    // serviceDeferredTrapClassifyWork().
    enqueueTrapClassifyDeferredWork({ trap_oid, trap, vpp_policer_index, true });

    return SAI_STATUS_SUCCESS;
}

void SwitchVpp::installTrapClassifyNow(
        _In_ sai_object_id_t trap_oid,
        _In_ const vpp_trap_entry_t &trap,
        _In_ uint32_t vpp_policer_index)
{
    SWSS_LOG_ENTER();

    if (vpp_policer_index == (uint32_t)~0)
    {
        SWSS_LOG_NOTICE("no VPP policer resolved for trap 0x%lx (trap_type %d); skipping classify install",
                (unsigned long)trap_oid, (int)trap.trap_type);

        return;
    }

    if (trap.trap_type == SAI_HOSTIF_TRAP_TYPE_IP2ME)
    {
        // IP2ME (and SNMP/SSH, which map to the same SAI trap type -- see
        // PROTOCOL_TO_TRAP_ID in sonic-mgmt's copp_tests.py) has no
        // device-input ethertype to match on: it identifies traffic by
        // destination IP after routing, not by L2 ethertype before it.
        // Bind the shared copp_ip2me_policer plugin's policer instead of
        // the ethertype-keyed copp_punt_policer path below -- see
        // copp_ip2me_policer.c for the full design (enforced on the
        // ip4-punt arc, ahead of ip4-punt-redirect).
        auto git = m_trap_group_map.find(trap.trap_group_oid);
        sai_object_id_t policer_oid = (git != m_trap_group_map.end()) ? git->second.policer_oid : SAI_NULL_OBJECT_ID;

        if (policer_oid != SAI_NULL_OBJECT_ID)
        {
            char policer_name[64];
            snprintf(policer_name, sizeof(policer_name), "copp-policer-0x%lx", (unsigned long)policer_oid);

            int pret = vpp_copp_ip2me_policer_bind(policer_name, true);

            if (pret != 0)
            {
                SWSS_LOG_ERROR("failed to bind IP2ME policer %s: ret %d", policer_name, pret);
            }
            else
            {
                SWSS_LOG_NOTICE("bound IP2ME policer %s for trap 0x%lx", policer_name, (unsigned long)trap_oid);
            }
        }

        return;
    }

    std::array<uint8_t, 16> match{};
    bool have_match_key = buildClassifyMatchForTrapType(trap.trap_type, match);

    if (!have_match_key)
    {
        SWSS_LOG_NOTICE("(no match-key mapping yet) would install punt+policer binding for "
                "SAI trap 0x%lx: trap_type %d, packet_action %d, vpp_policer_index %u",
                (unsigned long)trap_oid, (int)trap.trap_type, (int)trap.packet_action, vpp_policer_index);

        return;
    }

    // PRIMARY: device-input-arc plugin bind, by policer name (the same
    // "copp-policer-0x<oid>" name SwitchVppPolicer.cpp already registered
    // via vpp_policer_add_replace() for trap.trap_group_oid's bound
    // policer). Ethertype is the same 16-bit value buildClassifyMatchForTrapType()
    // just wrote into match[12:13] (big-endian on the wire there too).
    {
        uint16_t ethertype = (uint16_t)((match[12] << 8) | match[13]);

        auto git = m_trap_group_map.find(trap.trap_group_oid);
        sai_object_id_t policer_oid = (git != m_trap_group_map.end()) ? git->second.policer_oid : SAI_NULL_OBJECT_ID;

        if (policer_oid != SAI_NULL_OBJECT_ID)
        {
            char policer_name[64];
            snprintf(policer_name, sizeof(policer_name), "copp-policer-0x%lx", (unsigned long)policer_oid);

            int pret = vpp_copp_punt_policer_bind(ethertype, policer_name, true,
                    isIp4TtlExpiringTrap(trap.trap_type),
                    isIp6BgpTrap(trap.trap_type),
                    isIp6NdTrap(trap.trap_type));

            if (pret != 0)
            {
                SWSS_LOG_ERROR("failed to bind device-input policer for trap 0x%lx (trap_type %d, "
                        "ethertype 0x%04x, policer %s): ret %d",
                        (unsigned long)trap_oid, (int)trap.trap_type, ethertype, policer_name, pret);
            }
            else
            {
                SWSS_LOG_NOTICE("bound device-input policer for trap 0x%lx: trap_type %d, ethertype 0x%04x, "
                        "policer %s", (unsigned long)trap_oid, (int)trap.trap_type, ethertype, policer_name);
            }
        }
    }
}

sai_status_t SwitchVpp::uninstallTrapClassify(
        _In_ sai_object_id_t trap_oid,
        _In_ const vpp_trap_entry_t &trap)
{
    SWSS_LOG_ENTER();

    // Deferred (WD-timeout fix, generalized -- see enqueueTrapClassifyDeferredWork()
    // in SwitchVpp.h): same reasoning as installTrapClassify() above -- do not
    // call vpp_copp_punt_policer_bind() synchronously from inside a
    // watchdog-timed syncd SAI call (removeHostifTrap()/setHostifTrap()).
    enqueueTrapClassifyDeferredWork({ trap_oid, trap, (uint32_t)~0, false });

    return SAI_STATUS_SUCCESS;
}

void SwitchVpp::uninstallTrapClassifyNow(
        _In_ sai_object_id_t trap_oid,
        _In_ const vpp_trap_entry_t &trap)
{
    SWSS_LOG_ENTER();

    if (trap.trap_type == SAI_HOSTIF_TRAP_TYPE_IP2ME)
    {
        // Unlike the ethertype-keyed path, the IP2ME policer binding
        // isn't per-trap-instance -- there is exactly one SAI IP2ME trap
        // and one shared copp_ip2me_policer plugin policer, so removing
        // it here unconditionally unbinds. Address entries themselves
        // (which router-interface IPs are tracked) are managed separately
        // by vpp_copp_ip2me_policer_addr_add_del(), independent of trap
        // lifecycle.
        int pret = vpp_copp_ip2me_policer_bind("", false);

        if (pret != 0)
        {
            SWSS_LOG_ERROR("failed to unbind IP2ME policer for trap 0x%lx: ret %d",
                    (unsigned long)trap_oid, pret);
        }

        return;
    }

    std::array<uint8_t, 16> match{};
    bool have_match_key = buildClassifyMatchForTrapType(trap.trap_type, match);

    if (have_match_key)
    {
        uint16_t ethertype = (uint16_t)((match[12] << 8) | match[13]);

        // Multiple trap_types can share an ethertype (e.g.
        // SAI_HOSTIF_TRAP_TYPE_ARP_REQUEST/_RESPONSE both map to 0x0806),
        // but the device-input plugin's bind table is keyed purely by
        // ethertype with a single slot -- unbinding unconditionally here
        // would also silently unpolice any other still-installed trap on
        // the same ethertype. For the ARP-style (non-bgp/nd) case, only
        // actually unbind once no other tracked, classify_installed trap
        // still needs this exact ethertype. For the shared 0x86DD BGPV6/ND
        // slot, the VPP plugin tracks bgp/nd as independent sub-bindings
        // (see copp_punt_policer.h) that unbind independently -- so
        // BGPV6 and ND are each always unbound for real here regardless
        // of the other's state; "still_needed" only has to consider the
        // plain (non-bgp/nd) case.
        bool still_needed = false;

        if (!isIp6BgpTrap(trap.trap_type) && !isIp6NdTrap(trap.trap_type))
        {
            for (auto &kv : m_trap_map)
            {
                if (kv.first == trap_oid || !kv.second.classify_installed)
                {
                    continue;
                }

                std::array<uint8_t, 16> other_match{};

                if (buildClassifyMatchForTrapType(kv.second.trap_type, other_match) &&
                        other_match[12] == match[12] && other_match[13] == match[13] &&
                        !isIp6BgpTrap(kv.second.trap_type) && !isIp6NdTrap(kv.second.trap_type))
                {
                    still_needed = true;
                    break;
                }
            }
        }

        if (!still_needed)
        {
            vpp_copp_punt_policer_bind(ethertype, "", false,
                    isIp4TtlExpiringTrap(trap.trap_type),
                    isIp6BgpTrap(trap.trap_type),
                    isIp6NdTrap(trap.trap_type));
        }
        else
        {
            SWSS_LOG_NOTICE("ethertype 0x%04x still needed by another installed trap; "
                    "leaving device-input binding in place for SAI trap 0x%lx",
                    ethertype, (unsigned long)trap_oid);
        }
    }

    SWSS_LOG_NOTICE("unbound device-input policer for SAI trap 0x%lx: trap_type %d",
            (unsigned long)trap_oid, (int)trap.trap_type);
}

sai_status_t SwitchVpp::createHostifTrapGroup(
        _In_ sai_object_id_t object_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    auto sid = sai_serialize_object_id(object_id);

    CHECK_STATUS(create_internal(SAI_OBJECT_TYPE_HOSTIF_TRAP_GROUP, sid, switch_id, attr_count, attr_list));

    vpp_trap_group_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.admin_state = true; // SAI default for ADMIN_STATE is true

    for (uint32_t i = 0; i < attr_count; i++)
    {
        const sai_attribute_t &attr = attr_list[i];

        switch (attr.id)
        {
            case SAI_HOSTIF_TRAP_GROUP_ATTR_ADMIN_STATE:
                entry.admin_state = attr.value.booldata;
                break;

            case SAI_HOSTIF_TRAP_GROUP_ATTR_QUEUE:
                entry.queue = attr.value.u32;
                break;

            case SAI_HOSTIF_TRAP_GROUP_ATTR_POLICER:
                entry.policer_oid = attr.value.oid;
                break;

            default:
                break;
        }
    }

    m_trap_group_map[object_id] = entry;

    SWSS_LOG_NOTICE("created hostif trap group %s: admin_state %d, queue %u, policer 0x%lx",
            sid.c_str(), entry.admin_state, entry.queue, (unsigned long)entry.policer_oid);

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::removeHostifTrapGroup(
        _In_ const std::string &serializedObjectId)
{
    SWSS_LOG_ENTER();

    sai_object_id_t object_id;
    sai_deserialize_object_id(serializedObjectId, object_id);

    m_trap_group_map.erase(object_id);

    return remove_internal(SAI_OBJECT_TYPE_HOSTIF_TRAP_GROUP, serializedObjectId);
}

sai_status_t SwitchVpp::setHostifTrapGroup(
        _In_ const std::string &serializedObjectId,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    sai_object_id_t object_id;
    sai_deserialize_object_id(serializedObjectId, object_id);

    CHECK_STATUS(set_internal(SAI_OBJECT_TYPE_HOSTIF_TRAP_GROUP, serializedObjectId, attr));

    auto it = m_trap_group_map.find(object_id);

    if (it == m_trap_group_map.end())
    {
        vpp_trap_group_entry_t entry;
        memset(&entry, 0, sizeof(entry));
        entry.admin_state = true; // SAI default for ADMIN_STATE is true

        it = m_trap_group_map.emplace(object_id, entry).first;

        SWSS_LOG_NOTICE("lazily adopted untracked (switch-discovered) hostif trap group 0x%lx "
                "into m_trap_group_map", (unsigned long)object_id);
    }

    switch (attr->id)
    {
        case SAI_HOSTIF_TRAP_GROUP_ATTR_ADMIN_STATE:
            it->second.admin_state = attr->value.booldata;
            break;

        case SAI_HOSTIF_TRAP_GROUP_ATTR_QUEUE:
            it->second.queue = attr->value.u32;
            break;

        case SAI_HOSTIF_TRAP_GROUP_ATTR_POLICER:
            it->second.policer_oid = attr->value.oid;
            break;

        default:
            break;
    }

    // Trap group attribute changes (particularly a POLICER rebind) affect
    // every trap currently bound to this group; re-resolve their classify
    // bindings against the (possibly new) policer.
    for (auto &kv : m_trap_map)
    {
        if (kv.second.trap_group_oid == object_id)
        {
            uint32_t vpp_policer_index = (uint32_t)~0;
            auto pit = m_policer_map.find(it->second.policer_oid);
            if (pit != m_policer_map.end())
            {
                vpp_policer_index = pit->second.vpp_policer_index;
            }

            installTrapClassify(kv.first, kv.second, vpp_policer_index);
        }
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::createHostifTrap(
        _In_ sai_object_id_t object_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    auto sid = sai_serialize_object_id(object_id);

    CHECK_STATUS(create_internal(SAI_OBJECT_TYPE_HOSTIF_TRAP, sid, switch_id, attr_count, attr_list));

    vpp_trap_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    entry.packet_action = SAI_PACKET_ACTION_DROP; // SAI default

    for (uint32_t i = 0; i < attr_count; i++)
    {
        const sai_attribute_t &attr = attr_list[i];

        switch (attr.id)
        {
            case SAI_HOSTIF_TRAP_ATTR_TRAP_TYPE:
                entry.trap_type = (sai_hostif_trap_type_t)attr.value.s32;
                break;

            case SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION:
                entry.packet_action = (sai_packet_action_t)attr.value.s32;
                break;

            case SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP:
                entry.trap_group_oid = attr.value.oid;
                break;

            default:
                break;
        }
    }

    uint32_t vpp_policer_index = (uint32_t)~0;

    auto git = m_trap_group_map.find(entry.trap_group_oid);
    if (git != m_trap_group_map.end())
    {
        auto pit = m_policer_map.find(git->second.policer_oid);
        if (pit != m_policer_map.end())
        {
            vpp_policer_index = pit->second.vpp_policer_index;
        }
    }

    if (entry.packet_action == SAI_PACKET_ACTION_TRAP || entry.packet_action == SAI_PACKET_ACTION_COPY)
    {
        sai_status_t status = installTrapClassify(object_id, entry, vpp_policer_index);

        if (status == SAI_STATUS_SUCCESS)
        {
            entry.classify_installed = true;
        }
    }

    m_trap_map[object_id] = entry;

    SWSS_LOG_NOTICE("created hostif trap %s: trap_type %d, packet_action %d, trap_group 0x%lx",
            sid.c_str(), (int)entry.trap_type, (int)entry.packet_action, (unsigned long)entry.trap_group_oid);

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::removeHostifTrap(
        _In_ const std::string &serializedObjectId)
{
    SWSS_LOG_ENTER();

    sai_object_id_t object_id;
    sai_deserialize_object_id(serializedObjectId, object_id);

    auto it = m_trap_map.find(object_id);

    if (it != m_trap_map.end())
    {
        if (it->second.classify_installed)
        {
            uninstallTrapClassify(object_id, it->second);
        }

        m_trap_map.erase(it);
    }

    return remove_internal(SAI_OBJECT_TYPE_HOSTIF_TRAP, serializedObjectId);
}

sai_status_t SwitchVpp::setHostifTrap(
        _In_ const std::string &serializedObjectId,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    sai_object_id_t object_id;
    sai_deserialize_object_id(serializedObjectId, object_id);

    CHECK_STATUS(set_internal(SAI_OBJECT_TYPE_HOSTIF_TRAP, serializedObjectId, attr));

    auto it = m_trap_map.find(object_id);

    if (it == m_trap_map.end())
    {
        SWSS_LOG_ERROR("hostif trap 0x%lx not tracked in m_trap_map", (unsigned long)object_id);
        return SAI_STATUS_SUCCESS;
    }

    switch (attr->id)
    {
        case SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION:
            it->second.packet_action = (sai_packet_action_t)attr->value.s32;
            break;

        case SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP:
            it->second.trap_group_oid = attr->value.oid;
            break;

        default:
            break;
    }

    // Re-derive the punt/policer binding for this trap: this is the path
    // exercised by test_add_new_trap (packet_action DROP -> TRAP) and
    // test_remove_trap (packet_action TRAP/COPY -> DROP, or trap deleted
    // outright via removeHostifTrap above).
    bool should_be_installed =
        (it->second.packet_action == SAI_PACKET_ACTION_TRAP || it->second.packet_action == SAI_PACKET_ACTION_COPY);

    if (should_be_installed && !it->second.classify_installed)
    {
        uint32_t vpp_policer_index = (uint32_t)~0;

        auto git = m_trap_group_map.find(it->second.trap_group_oid);
        if (git != m_trap_group_map.end())
        {
            auto pit = m_policer_map.find(git->second.policer_oid);
            if (pit != m_policer_map.end())
            {
                vpp_policer_index = pit->second.vpp_policer_index;
            }
        }

        if (installTrapClassify(object_id, it->second, vpp_policer_index) == SAI_STATUS_SUCCESS)
        {
            it->second.classify_installed = true;
        }
    }
    else if (!should_be_installed && it->second.classify_installed)
    {
        if (uninstallTrapClassify(object_id, it->second) == SAI_STATUS_SUCCESS)
        {
            it->second.classify_installed = false;
        }
    }
    else if (should_be_installed && it->second.classify_installed && attr->id == SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP)
    {
        // Trap stays installed but moved to a different trap group: neither
        // branch above fires (classify_installed doesn't change), so without
        // this the trap would silently keep its old group's policer binding.
        // installTrapClassify()/vpp_copp_punt_policer_bind() are idempotent
        // on the same ethertype -- re-running with the new group's policer
        // just updates which policer name that ethertype resolves to.
        uint32_t vpp_policer_index = (uint32_t)~0;

        auto git = m_trap_group_map.find(it->second.trap_group_oid);
        if (git != m_trap_group_map.end())
        {
            auto pit = m_policer_map.find(git->second.policer_oid);
            if (pit != m_policer_map.end())
            {
                vpp_policer_index = pit->second.vpp_policer_index;
            }
        }

        installTrapClassify(object_id, it->second, vpp_policer_index);
    }

    return SAI_STATUS_SUCCESS;
}
