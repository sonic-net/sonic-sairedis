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
#include <vector>
#include <tuple>

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
        SWSS_LOG_ENTER();

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

            default:
                return false;
        }


        match[12] = (uint8_t)(ethertype >> 8);
        match[13] = (uint8_t)(ethertype & 0xFF);

        return true;
    }

    bool isIp4TtlExpiringTrap(sai_hostif_trap_type_t trap_type)
    {
        SWSS_LOG_ENTER();

        return trap_type == SAI_HOSTIF_TRAP_TYPE_TTL_ERROR;
    }

    bool isBgpTrap(sai_hostif_trap_type_t trap_type)
    {
        SWSS_LOG_ENTER();

        return trap_type == SAI_HOSTIF_TRAP_TYPE_BGP || trap_type == SAI_HOSTIF_TRAP_TYPE_BGPV6;
    }

}


/*
 * Install (or update) the VPP-native punt-path policer binding for `trap`
 */
sai_status_t SwitchVpp::installTrapClassify(
        _In_ sai_object_id_t trap_oid,
        _In_ const vpp_trap_entry_t &trap,
        _In_ uint32_t vpp_policer_index)
{
    SWSS_LOG_ENTER();

    return installTrapClassifyNow(trap_oid, trap, vpp_policer_index);
}

sai_status_t SwitchVpp::installTrapClassifyNow(
        _In_ sai_object_id_t trap_oid,
        _In_ const vpp_trap_entry_t &trap,
        _In_ uint32_t vpp_policer_index)
{
    SWSS_LOG_ENTER();

    if (vpp_policer_index == (uint32_t)~0)
    {
        SWSS_LOG_NOTICE("no VPP policer resolved for trap 0x%lx (trap_type %d); skipping classify install",
                (unsigned long)trap_oid, (int)trap.trap_type);

        return SAI_STATUS_SUCCESS;
    }

    if (isBgpTrap(trap.trap_type))
    {
        // BGP/BGPV6: independent policer slot, matched by TCP dst port 179
        sai_object_id_t policer_oid = SAI_NULL_OBJECT_ID;
        {
            std::lock_guard<std::mutex> lock(m_copp_state_mutex);
            auto git = m_trap_group_map.find(trap.trap_group_oid);
            policer_oid = (git != m_trap_group_map.end()) ? git->second.policer_oid : SAI_NULL_OBJECT_ID;
        }

        if (policer_oid != SAI_NULL_OBJECT_ID)
        {
            char policer_name[64];
            snprintf(policer_name, sizeof(policer_name), "copp-policer-0x%lx", (unsigned long)policer_oid);

            int pret = vpp_sonic_ext_copp_ip2me_bind_condition(policer_name, 179, true);

            if (pret != 0)
            {
                SWSS_LOG_ERROR("failed to bind BGP policer %s: ret %d", policer_name, pret);
                return SAI_STATUS_FAILURE;
            }
            else
            {
                SWSS_LOG_NOTICE("bound BGP policer %s for trap 0x%lx", policer_name, (unsigned long)trap_oid);
            }
        }

        return SAI_STATUS_SUCCESS;
    }

    if (trap.trap_type == SAI_HOSTIF_TRAP_TYPE_IP2ME)
    {
        // IP2ME identifies traffic by destination IP after routing, not by L2 ethertype before it.
        sai_object_id_t policer_oid = SAI_NULL_OBJECT_ID;
        {
            std::lock_guard<std::mutex> lock(m_copp_state_mutex);
            auto git = m_trap_group_map.find(trap.trap_group_oid);
            policer_oid = (git != m_trap_group_map.end()) ? git->second.policer_oid : SAI_NULL_OBJECT_ID;
        }

        if (policer_oid != SAI_NULL_OBJECT_ID)
        {
            char policer_name[64];
            snprintf(policer_name, sizeof(policer_name), "copp-policer-0x%lx", (unsigned long)policer_oid);

            int pret = vpp_sonic_ext_copp_ip2me_bind(policer_name, true);

            if (pret != 0)
            {
                SWSS_LOG_ERROR("failed to bind IP2ME policer %s: ret %d", policer_name, pret);
                return SAI_STATUS_FAILURE;
            }
            else
            {
                SWSS_LOG_NOTICE("bound IP2ME policer %s for trap 0x%lx", policer_name, (unsigned long)trap_oid);
            }
        }

        return SAI_STATUS_SUCCESS;
    }

    std::array<uint8_t, 16> match{};
    bool have_match_key = buildClassifyMatchForTrapType(trap.trap_type, match);

    if (!have_match_key)
    {
        SWSS_LOG_NOTICE("(no match-key mapping yet) would install punt+policer binding for "
                "SAI trap 0x%lx: trap_type %d, packet_action %d, vpp_policer_index %u",
                (unsigned long)trap_oid, (int)trap.trap_type, (int)trap.packet_action, vpp_policer_index);

        return SAI_STATUS_SUCCESS;
    }

    // interface-output-arc feature bind by policer name
    {
        uint16_t ethertype = (uint16_t)((match[12] << 8) | match[13]);

        sai_object_id_t policer_oid = SAI_NULL_OBJECT_ID;
        {
            std::lock_guard<std::mutex> lock(m_copp_state_mutex);
            auto git = m_trap_group_map.find(trap.trap_group_oid);
            policer_oid = (git != m_trap_group_map.end()) ? git->second.policer_oid : SAI_NULL_OBJECT_ID;
        }

        if (policer_oid != SAI_NULL_OBJECT_ID)
        {
            char policer_name[64];
            snprintf(policer_name, sizeof(policer_name), "copp-policer-0x%lx", (unsigned long)policer_oid);

            int pret = vpp_sonic_ext_copp_ifout_bind(ethertype, policer_name, true,
                    isIp4TtlExpiringTrap(trap.trap_type));

            if (pret != 0)
            {
                SWSS_LOG_ERROR("failed to bind copp-ifout policer for trap 0x%lx (trap_type %d, "
                        "ethertype 0x%04x, policer %s): ret %d",
                        (unsigned long)trap_oid, (int)trap.trap_type, ethertype, policer_name, pret);
                return SAI_STATUS_FAILURE;
            }
            else
            {
                SWSS_LOG_NOTICE("bound copp-ifout policer for trap 0x%lx: trap_type %d, ethertype 0x%04x, "
                        "policer %s", (unsigned long)trap_oid, (int)trap.trap_type, ethertype, policer_name);
            }

            // TTL_ERROR additionally needs the punt-to-host path enabled: without
            // this, genuinely-transiting TTL-expired packets never reach
            // copp-ifout at all (they dead-end in VPP's stock ip4-icmp-error path,
            // which never punts anywhere).
            if (isIp4TtlExpiringTrap(trap.trap_type))
            {
                int tret = vpp_sonic_ext_copp_ttl_punt_bind(true);

                if (tret != 0)
                {
                    SWSS_LOG_ERROR("failed to enable TTL_ERROR punt-to-host for trap 0x%lx: ret %d",
                            (unsigned long)trap_oid, tret);
                    return SAI_STATUS_FAILURE;
                }
                else
                {
                    SWSS_LOG_NOTICE("enabled TTL_ERROR punt-to-host for trap 0x%lx", (unsigned long)trap_oid);
                }
            }
        }
    }

    return SAI_STATUS_SUCCESS;
}


sai_status_t SwitchVpp::uninstallTrapClassify(
        _In_ sai_object_id_t trap_oid,
        _In_ const vpp_trap_entry_t &trap)
{
    SWSS_LOG_ENTER();

    return uninstallTrapClassifyNow(trap_oid, trap);
}

sai_status_t SwitchVpp::uninstallTrapClassifyNow(
        _In_ sai_object_id_t trap_oid,
        _In_ const vpp_trap_entry_t &trap)
{
    SWSS_LOG_ENTER();

    if (isBgpTrap(trap.trap_type))
    {
        // BGP/BGPV6: unbind ONLY this trap's own TCP-dst-port-179 slot
        sai_object_id_t policer_oid = SAI_NULL_OBJECT_ID;
        {
            std::lock_guard<std::mutex> lock(m_copp_state_mutex);
            auto git = m_trap_group_map.find(trap.trap_group_oid);
            policer_oid = (git != m_trap_group_map.end()) ? git->second.policer_oid : SAI_NULL_OBJECT_ID;
        }

        if (policer_oid != SAI_NULL_OBJECT_ID)
        {
            char policer_name[64];
            snprintf(policer_name, sizeof(policer_name), "copp-policer-0x%lx", (unsigned long)policer_oid);

            int pret = vpp_sonic_ext_copp_ip2me_bind_condition(policer_name, 179, false);

            if (pret != 0)
            {
                SWSS_LOG_ERROR("failed to unbind BGP policer %s for trap 0x%lx: ret %d",
                        policer_name, (unsigned long)trap_oid, pret);
                return SAI_STATUS_FAILURE;
            }
        }

        return SAI_STATUS_SUCCESS;
    }

    if (trap.trap_type == SAI_HOSTIF_TRAP_TYPE_IP2ME)
    {
        sai_object_id_t policer_oid = SAI_NULL_OBJECT_ID;
        {
            std::lock_guard<std::mutex> lock(m_copp_state_mutex);
            auto git = m_trap_group_map.find(trap.trap_group_oid);
            policer_oid = (git != m_trap_group_map.end()) ? git->second.policer_oid : SAI_NULL_OBJECT_ID;
        }

        int pret = SAI_STATUS_SUCCESS;

        if (policer_oid != SAI_NULL_OBJECT_ID)
        {
            char policer_name[64];
            snprintf(policer_name, sizeof(policer_name), "copp-policer-0x%lx", (unsigned long)policer_oid);

            pret = vpp_sonic_ext_copp_ip2me_bind(policer_name, false);
        }

        if (pret != 0)
        {
            SWSS_LOG_ERROR("failed to unbind IP2ME policer for trap 0x%lx: ret %d",
                    (unsigned long)trap_oid, pret);
            return SAI_STATUS_FAILURE;
        }

        return SAI_STATUS_SUCCESS;
    }

    std::array<uint8_t, 16> match{};
    bool have_match_key = buildClassifyMatchForTrapType(trap.trap_type, match);

    if (have_match_key)
    {
        uint16_t ethertype = (uint16_t)((match[12] << 8) | match[13]);

        // Multiple trap_types can share an ethertype, but sonic-ext-copp-ifout's
        // bind table is keyed by ethertype with a single slot -- unbinding
        // unconditionally here would also silently unpolice any other still-installed
        // trap on the same ethertype. Unbind once no other trap still needs this ethertype.
        bool still_needed = false;
        {
            std::lock_guard<std::mutex> lock(m_copp_state_mutex);
            for (auto &kv : m_trap_map)
            {
                if (kv.first == trap_oid || !kv.second.classify_installed)
                {
                    continue;
                }

                std::array<uint8_t, 16> other_match{};

                if (buildClassifyMatchForTrapType(kv.second.trap_type, other_match) &&
                        other_match[12] == match[12] && other_match[13] == match[13])
                {
                    still_needed = true;
                    break;
                }
            }
        }

        if (!still_needed)
        {
            int pret = vpp_sonic_ext_copp_ifout_bind(ethertype, "", false,
                    isIp4TtlExpiringTrap(trap.trap_type));

            if (pret != 0)
            {
                SWSS_LOG_ERROR("failed to unbind copp-ifout policer for trap 0x%lx "
                        "(ethertype 0x%04x): ret %d",
                        (unsigned long)trap_oid, ethertype, pret);
                return SAI_STATUS_FAILURE;
            }

            // Disable the punt-to-host toggle alongside the copp-ifout unbind
            // above. A stale enable here would leave ip4-rewrite redirecting
	    // transit TTL-expired traffic to a policer that's just been unbound.
            if (isIp4TtlExpiringTrap(trap.trap_type))
            {
                int tret = vpp_sonic_ext_copp_ttl_punt_bind(false);

                if (tret != 0)
                {
                    SWSS_LOG_ERROR("failed to disable TTL_ERROR punt-to-host for trap 0x%lx: ret %d",
                            (unsigned long)trap_oid, tret);
                    return SAI_STATUS_FAILURE;
                }
            }
        }
        else
        {
            SWSS_LOG_NOTICE("ethertype 0x%04x still needed by another installed trap; "
                        "leaving copp-ifout binding in place for SAI trap 0x%lx",
                        ethertype, (unsigned long)trap_oid);
        }
    }

    SWSS_LOG_NOTICE("unbound policer for SAI trap 0x%lx: trap_type %d",
            (unsigned long)trap_oid, (int)trap.trap_type);

    return SAI_STATUS_SUCCESS;
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

    {
        std::lock_guard<std::mutex> lock(m_copp_state_mutex);
        m_trap_group_map[object_id] = entry;
    }

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

    {
        std::lock_guard<std::mutex> lock(m_copp_state_mutex);
        m_trap_group_map.erase(object_id);
    }

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

    // Snapshot each affected trap while holding the state lock, then perform
    // the synchronous VPP RPCs after releasing it.
    std::vector<std::tuple<sai_object_id_t, vpp_trap_entry_t, uint32_t>> to_reinstall;

    {
        std::lock_guard<std::mutex> lock(m_copp_state_mutex);

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

                to_reinstall.emplace_back(kv.first, kv.second, vpp_policer_index);
            }
        }
    }

    for (auto &t : to_reinstall)
    {
        sai_status_t status = installTrapClassify(
                std::get<0>(t), std::get<1>(t), std::get<2>(t));

        if (status != SAI_STATUS_SUCCESS)
        {
            return status;
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

    {
        std::lock_guard<std::mutex> lock(m_copp_state_mutex);

        auto git = m_trap_group_map.find(entry.trap_group_oid);
        if (git != m_trap_group_map.end())
        {
            auto pit = m_policer_map.find(git->second.policer_oid);
            if (pit != m_policer_map.end())
            {
                vpp_policer_index = pit->second.vpp_policer_index;
            }
        }
    }

    if (entry.packet_action == SAI_PACKET_ACTION_TRAP || entry.packet_action == SAI_PACKET_ACTION_COPY)
    {
        CHECK_STATUS(installTrapClassify(object_id, entry, vpp_policer_index));
        entry.classify_installed = true;
    }

    {
        std::lock_guard<std::mutex> lock(m_copp_state_mutex);
        m_trap_map[object_id] = entry;
    }

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

    bool need_uninstall = false;
    vpp_trap_entry_t trap_copy;
    memset(&trap_copy, 0, sizeof(trap_copy));

    {
        std::lock_guard<std::mutex> lock(m_copp_state_mutex);

        auto it = m_trap_map.find(object_id);

        if (it != m_trap_map.end() && it->second.classify_installed)
        {
            need_uninstall = true;
            trap_copy = it->second;
        }
    }

    if (need_uninstall)
    {
        CHECK_STATUS(uninstallTrapClassify(object_id, trap_copy));
    }

    {
        std::lock_guard<std::mutex> lock(m_copp_state_mutex);
        m_trap_map.erase(object_id);
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

    bool need_install = false;
    bool need_uninstall = false;
    uint32_t vpp_policer_index = (uint32_t)~0;
    vpp_trap_entry_t trap_copy;
    memset(&trap_copy, 0, sizeof(trap_copy));

    {
        std::lock_guard<std::mutex> lock(m_copp_state_mutex);

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

        bool should_be_installed =
            (it->second.packet_action == SAI_PACKET_ACTION_TRAP ||
             it->second.packet_action == SAI_PACKET_ACTION_COPY);

        need_install = should_be_installed &&
            (!it->second.classify_installed || attr->id == SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP);
        need_uninstall = !should_be_installed && it->second.classify_installed;

        if (need_install)
        {
            auto git = m_trap_group_map.find(it->second.trap_group_oid);
            if (git != m_trap_group_map.end())
            {
                auto pit = m_policer_map.find(git->second.policer_oid);
                if (pit != m_policer_map.end())
                {
                    vpp_policer_index = pit->second.vpp_policer_index;
                }
            }
        }

        trap_copy = it->second;
    }

    if (need_install)
    {
        CHECK_STATUS(installTrapClassify(object_id, trap_copy, vpp_policer_index));
    }
    else if (need_uninstall)
    {
        CHECK_STATUS(uninstallTrapClassify(object_id, trap_copy));
    }

    if (need_install || need_uninstall)
    {
        std::lock_guard<std::mutex> lock(m_copp_state_mutex);
        auto it = m_trap_map.find(object_id);
        if (it != m_trap_map.end())
        {
            it->second.classify_installed = need_install;
        }
    }

    return SAI_STATUS_SUCCESS;
}
