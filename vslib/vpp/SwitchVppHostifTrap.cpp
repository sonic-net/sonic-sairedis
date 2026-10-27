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
#include <utility>
#include <vector>

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

    bool isNeighborMissTrap(sai_hostif_trap_type_t trap_type)
    {
        SWSS_LOG_ENTER();

        return trap_type == SAI_HOSTIF_TRAP_TYPE_NEIGHBOR_MISS;
    }

    bool trapsShareBinding(
            _In_ sai_hostif_trap_type_t lhs,
            _In_ sai_hostif_trap_type_t rhs)
    {
        SWSS_LOG_ENTER();

        if (isNeighborMissTrap(lhs) || isNeighborMissTrap(rhs))
        {
            return isNeighborMissTrap(lhs) && isNeighborMissTrap(rhs);
        }

        if (isBgpTrap(lhs) || isBgpTrap(rhs))
        {
            return isBgpTrap(lhs) && isBgpTrap(rhs);
        }

        if (lhs == SAI_HOSTIF_TRAP_TYPE_IP2ME ||
                rhs == SAI_HOSTIF_TRAP_TYPE_IP2ME)
        {
            return lhs == SAI_HOSTIF_TRAP_TYPE_IP2ME &&
                rhs == SAI_HOSTIF_TRAP_TYPE_IP2ME;
        }

        std::array<uint8_t, 16> lhs_match{};
        std::array<uint8_t, 16> rhs_match{};

        return buildClassifyMatchForTrapType(lhs, lhs_match) &&
            buildClassifyMatchForTrapType(rhs, rhs_match) &&
            lhs_match[12] == rhs_match[12] &&
            lhs_match[13] == rhs_match[13];
    }

    bool trapBindingKey(
            _In_ sai_hostif_trap_type_t trap_type,
            _Out_ uint32_t &key)
    {
        SWSS_LOG_ENTER();

        if (isNeighborMissTrap(trap_type))
        {
            key = 0x10000;
            return true;
        }

        if (isBgpTrap(trap_type))
        {
            key = 0x10001;
            return true;
        }

        if (trap_type == SAI_HOSTIF_TRAP_TYPE_IP2ME)
        {
            key = 0x10002;
            return true;
        }

        std::array<uint8_t, 16> match{};
        if (!buildClassifyMatchForTrapType(trap_type, match))
        {
            return false;
        }

        key = 0x20000 | ((uint32_t)match[12] << 8) | (uint32_t)match[13];
        if (isIp4TtlExpiringTrap(trap_type))
        {
            key |= 0x100000;
        }
        return true;
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

    sai_object_id_t policer_oid = SAI_NULL_OBJECT_ID;
    {
        std::lock_guard<std::mutex> lock(m_copp_state_mutex);
        auto git = m_trap_group_map.find(trap.trap_group_oid);
        policer_oid = (git != m_trap_group_map.end()) ?
            git->second.policer_oid : SAI_NULL_OBJECT_ID;
    }

    return installTrapClassifyNow(
            trap_oid, trap, vpp_policer_index, policer_oid);
}

sai_status_t SwitchVpp::installTrapClassifyNow(
        _In_ sai_object_id_t trap_oid,
        _In_ const vpp_trap_entry_t &trap,
        _In_ uint32_t vpp_policer_index,
        _In_ sai_object_id_t policer_oid,
        _In_ sai_object_id_t excluded_trap_group_oid)
{
    SWSS_LOG_ENTER();

    uint32_t old_vpp_policer_index = (uint32_t)~0;
    sai_object_id_t old_policer_oid = SAI_NULL_OBJECT_ID;
    uint32_t binding_key = 0;

    {
        std::lock_guard<std::mutex> lock(m_copp_state_mutex);
        if (trapBindingKey(trap.trap_type, binding_key))
        {
            auto bit = m_trap_binding_map.find(binding_key);
            if (bit != m_trap_binding_map.end())
            {
                old_policer_oid = bit->second;
                auto pit = m_policer_map.find(old_policer_oid);
                if (pit != m_policer_map.end())
                {
                    old_vpp_policer_index = pit->second.vpp_policer_index;
                }
            }
        }
    }

    sai_status_t status = programTrapClassifyNow(
            trap_oid, trap, vpp_policer_index, policer_oid,
            excluded_trap_group_oid);

    if (status == SAI_STATUS_SUCCESS)
    {
        return status;
    }

    // TTL_ERROR and future compound bindings can fail after changing an
    // earlier VPP component. Restore this row before returning the failure.
    sai_status_t rollback_status = programTrapClassifyNow(
            trap_oid, trap, old_vpp_policer_index, old_policer_oid,
            SAI_NULL_OBJECT_ID, false);

    if (rollback_status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("failed to restore trap 0x%lx after classify install "
                "failure: status %d", (unsigned long)trap_oid,
                rollback_status);
        return rollback_status;
    }

    return status;
}

sai_status_t SwitchVpp::programTrapClassifyNow(
        _In_ sai_object_id_t trap_oid,
        _In_ const vpp_trap_entry_t &trap,
        _In_ uint32_t vpp_policer_index,
        _In_ sai_object_id_t policer_oid,
        _In_ sai_object_id_t excluded_trap_group_oid,
        _In_ bool reconcile_shared_owners)
{
    SWSS_LOG_ENTER();

    uint32_t binding_key = 0;
    bool have_binding_key = trapBindingKey(trap.trap_type, binding_key);
    sai_object_id_t current_policer_oid = SAI_NULL_OBJECT_ID;
    {
        std::lock_guard<std::mutex> lock(m_copp_state_mutex);
        auto bit = have_binding_key ? m_trap_binding_map.find(binding_key) :
            m_trap_binding_map.end();
        if (bit != m_trap_binding_map.end())
        {
            current_policer_oid = bit->second;
        }
    }

    auto record_binding = [&](sai_object_id_t programmed_policer_oid)
    {
        if (!have_binding_key)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(m_copp_state_mutex);
        if (programmed_policer_oid == SAI_NULL_OBJECT_ID)
        {
            m_trap_binding_map.erase(binding_key);
        }
        else
        {
            m_trap_binding_map[binding_key] = programmed_policer_oid;
        }
    };

    // A VPP row can be shared by multiple SAI traps. If this trap has no
    // desired policer, select another installed owner before deleting the row.
    if (reconcile_shared_owners &&
            (vpp_policer_index == (uint32_t)~0 ||
             policer_oid == SAI_NULL_OBJECT_ID))
    {
        std::lock_guard<std::mutex> lock(m_copp_state_mutex);

        for (const auto &kv : m_trap_map)
        {
            if (kv.first == trap_oid || !kv.second.classify_installed ||
                    (excluded_trap_group_oid != SAI_NULL_OBJECT_ID &&
                     kv.second.trap_group_oid == excluded_trap_group_oid) ||
                    !trapsShareBinding(trap.trap_type, kv.second.trap_type))
            {
                continue;
            }

            auto git = m_trap_group_map.find(kv.second.trap_group_oid);
            if (git == m_trap_group_map.end() ||
                    git->second.policer_oid == SAI_NULL_OBJECT_ID)
            {
                continue;
            }

            auto pit = m_policer_map.find(git->second.policer_oid);
            if (pit != m_policer_map.end())
            {
                policer_oid = git->second.policer_oid;
                vpp_policer_index = pit->second.vpp_policer_index;
                break;
            }
        }
    }

    bool have_policer = vpp_policer_index != (uint32_t)~0 &&
        policer_oid != SAI_NULL_OBJECT_ID;

    if (isNeighborMissTrap(trap.trap_type))
    {
        // NEIGHBOR_MISS is a singleton semantic row. A trap-group update to
        // SAI_NULL_OBJECT_ID (or to an unresolved policer) must remove the old
        // row instead of taking the generic "nothing to install" exit below.
        if (!have_policer)
        {
            int pret = vpp_sonic_ext_copp_neighbor_miss_bind("", false);

            if (pret != 0)
            {
                SWSS_LOG_ERROR("failed to unbind unresolved NEIGHBOR_MISS policer for "
                        "trap 0x%lx: ret %d", (unsigned long)trap_oid, pret);
                return SAI_STATUS_FAILURE;
            }

            SWSS_LOG_NOTICE("unbound unresolved NEIGHBOR_MISS policer for trap 0x%lx",
                    (unsigned long)trap_oid);
            record_binding(SAI_NULL_OBJECT_ID);
            return SAI_STATUS_SUCCESS;
        }

        // Neighbor miss is identified after routing by sonic_ext's
        // glean-redirect node, not by a wire ethertype. Bind its trap-group
        // policer to the dedicated semantic row; copp-ifout selects this row
        // only when the packet carries the explicit per-buffer reason.
        char policer_name[64];
        snprintf(policer_name, sizeof(policer_name), "copp-policer-0x%lx",
                (unsigned long)policer_oid);

        int pret = vpp_sonic_ext_copp_neighbor_miss_bind(policer_name, true);

        if (pret != 0)
        {
            SWSS_LOG_ERROR("failed to bind NEIGHBOR_MISS policer %s for trap 0x%lx: ret %d",
                    policer_name, (unsigned long)trap_oid, pret);
            return SAI_STATUS_FAILURE;
        }

        SWSS_LOG_NOTICE("bound NEIGHBOR_MISS policer %s for trap 0x%lx",
                policer_name, (unsigned long)trap_oid);
        record_binding(policer_oid);
        return SAI_STATUS_SUCCESS;
    }

    if (isBgpTrap(trap.trap_type))
    {
        if (!have_policer)
        {
            if (current_policer_oid == SAI_NULL_OBJECT_ID)
            {
                return SAI_STATUS_SUCCESS;
            }

            char policer_name[64];
            snprintf(policer_name, sizeof(policer_name),
                    "copp-policer-0x%lx",
                    (unsigned long)current_policer_oid);
            int pret = vpp_sonic_ext_copp_ip2me_bind_condition(
                    policer_name, 179, false);
            if (pret != 0)
            {
                return SAI_STATUS_FAILURE;
            }
            record_binding(SAI_NULL_OBJECT_ID);
            return SAI_STATUS_SUCCESS;
        }

        char policer_name[64];
        snprintf(policer_name, sizeof(policer_name), "copp-policer-0x%lx",
                (unsigned long)policer_oid);
        int pret = vpp_sonic_ext_copp_ip2me_bind_condition(
                policer_name, 179, true);

        if (pret != 0)
        {
            SWSS_LOG_ERROR("failed to bind BGP policer for trap 0x%lx: ret %d",
                    (unsigned long)trap_oid, pret);
            return SAI_STATUS_FAILURE;
        }

        record_binding(policer_oid);
        return SAI_STATUS_SUCCESS;
    }

    if (trap.trap_type == SAI_HOSTIF_TRAP_TYPE_IP2ME)
    {
        if (!have_policer)
        {
            if (current_policer_oid == SAI_NULL_OBJECT_ID)
            {
                return SAI_STATUS_SUCCESS;
            }

            char policer_name[64];
            snprintf(policer_name, sizeof(policer_name),
                    "copp-policer-0x%lx",
                    (unsigned long)current_policer_oid);
            int pret = vpp_sonic_ext_copp_ip2me_bind(
                    policer_name, false);
            if (pret != 0)
            {
                return SAI_STATUS_FAILURE;
            }
            record_binding(SAI_NULL_OBJECT_ID);
            return SAI_STATUS_SUCCESS;
        }

        char policer_name[64];
        snprintf(policer_name, sizeof(policer_name), "copp-policer-0x%lx",
                (unsigned long)policer_oid);
        int pret = vpp_sonic_ext_copp_ip2me_bind(policer_name, true);

        if (pret != 0)
        {
            SWSS_LOG_ERROR("failed to bind IP2ME policer for trap 0x%lx: ret %d",
                    (unsigned long)trap_oid, pret);
            return SAI_STATUS_FAILURE;
        }

        record_binding(policer_oid);
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

        char policer_name[64] = {};
        if (have_policer)
        {
            snprintf(policer_name, sizeof(policer_name), "copp-policer-0x%lx",
                    (unsigned long)policer_oid);
        }

        int pret = vpp_sonic_ext_copp_ifout_bind(
                ethertype, policer_name, have_policer,
                isIp4TtlExpiringTrap(trap.trap_type));

        if (pret != 0)
        {
            SWSS_LOG_ERROR("failed to %s copp-ifout policer for trap 0x%lx "
                    "(trap_type %d, ethertype 0x%04x): ret %d",
                    have_policer ? "bind" : "unbind",
                    (unsigned long)trap_oid, (int)trap.trap_type,
                    ethertype, pret);
            return SAI_STATUS_FAILURE;
        }

        // TTL_ERROR additionally needs the punt-to-host path toggled: without
        // this, genuinely-transiting TTL-expired packets never reach
        // copp-ifout at all.
        if (isIp4TtlExpiringTrap(trap.trap_type))
        {
            int tret = vpp_sonic_ext_copp_ttl_punt_bind(have_policer);

            if (tret != 0)
            {
                SWSS_LOG_ERROR("failed to %s TTL_ERROR punt-to-host for trap 0x%lx: ret %d",
                        have_policer ? "enable" : "disable",
                        (unsigned long)trap_oid, tret);
                return SAI_STATUS_FAILURE;
            }
        }

        record_binding(have_policer ? policer_oid : SAI_NULL_OBJECT_ID);
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

    // Reconcile this shared VPP row to another installed owner, or remove it
    // when no owner remains. installTrapClassifyNow() snapshots and restores
    // the old state if a compound uninstall fails part-way through.
    return installTrapClassifyNow(trap_oid, trap, (uint32_t)~0,
            SAI_NULL_OBJECT_ID, SAI_NULL_OBJECT_ID);
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

    // Build the prospective state without publishing it. VPP must accept every
    // affected rebind before set_internal() or the replay cache is changed.
    vpp_trap_group_entry_t old_entry{};
    old_entry.admin_state = true; // SAI default
    vpp_trap_group_entry_t new_entry{};
    std::vector<std::pair<sai_object_id_t, vpp_trap_entry_t>> to_reinstall;
    std::map<uint32_t, std::pair<sai_object_id_t, uint32_t>> old_bindings;
    std::map<uint32_t, std::pair<sai_object_id_t, vpp_trap_entry_t>>
        binding_representatives;
    uint32_t new_vpp_policer_index = (uint32_t)~0;

    {
        std::lock_guard<std::mutex> lock(m_copp_state_mutex);

        auto it = m_trap_group_map.find(object_id);

        if (it != m_trap_group_map.end())
        {
            old_entry = it->second;
        }

        new_entry = old_entry;

        switch (attr->id)
        {
            case SAI_HOSTIF_TRAP_GROUP_ATTR_ADMIN_STATE:
                new_entry.admin_state = attr->value.booldata;
                break;

            case SAI_HOSTIF_TRAP_GROUP_ATTR_QUEUE:
                new_entry.queue = attr->value.u32;
                break;

            case SAI_HOSTIF_TRAP_GROUP_ATTR_POLICER:
                new_entry.policer_oid = attr->value.oid;
                break;

            default:
                break;
        }

        auto new_pit = m_policer_map.find(new_entry.policer_oid);
        if (new_pit != m_policer_map.end())
        {
            new_vpp_policer_index = new_pit->second.vpp_policer_index;
        }

        // Trap group attribute changes (particularly a POLICER rebind) affect
        // every trap currently bound to this group; re-resolve their classify
        // bindings against the prospective policer.
        for (auto &kv : m_trap_map)
        {
            if (kv.second.trap_group_oid == object_id && kv.second.classify_installed)
            {
                to_reinstall.emplace_back(kv.first, kv.second);

                uint32_t key = 0;
                if (trapBindingKey(kv.second.trap_type, key))
                {
                    binding_representatives.emplace(key,
                            std::make_pair(kv.first, kv.second));
                    sai_object_id_t effective_oid = SAI_NULL_OBJECT_ID;
                    uint32_t effective_index = (uint32_t)~0;
                    auto bit = m_trap_binding_map.find(key);
                    if (bit != m_trap_binding_map.end())
                    {
                        effective_oid = bit->second;
                        auto pit = m_policer_map.find(effective_oid);
                        if (pit != m_policer_map.end())
                        {
                            effective_index = pit->second.vpp_policer_index;
                        }
                    }
                    old_bindings[key] = std::make_pair(
                            effective_oid, effective_index);
                }
            }
        }
    }

    auto rollback_programmed = [&]()
    {
        for (const auto &binding : old_bindings)
        {
            {
                std::lock_guard<std::mutex> lock(m_copp_state_mutex);
                auto current = m_trap_binding_map.find(binding.first);
                sai_object_id_t current_oid = current == m_trap_binding_map.end() ?
                    SAI_NULL_OBJECT_ID : current->second;
                if (current_oid == binding.second.first)
                {
                    continue;
                }
            }

            const auto &t = binding_representatives.at(binding.first);
            sai_status_t rollback_status = programTrapClassifyNow(
                    t.first, t.second, binding.second.second,
                    binding.second.first, SAI_NULL_OBJECT_ID, false);

            if (rollback_status != SAI_STATUS_SUCCESS)
            {
                SWSS_LOG_ERROR("failed to restore binding 0x%x while rolling "
                        "back trap-group 0x%lx update: status %d",
                        binding.first, (unsigned long)object_id,
                        rollback_status);
            }
        }
    };

    for (const auto &t : to_reinstall)
    {
        sai_status_t status = installTrapClassifyNow(
                t.first, t.second, new_vpp_policer_index,
                new_entry.policer_oid, object_id);

        if (status != SAI_STATUS_SUCCESS)
        {
            rollback_programmed();
            return status;
        }
    }

    sai_status_t status = set_internal(
            SAI_OBJECT_TYPE_HOSTIF_TRAP_GROUP, serializedObjectId, attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        rollback_programmed();
        return status;
    }

    {
        std::lock_guard<std::mutex> lock(m_copp_state_mutex);
        m_trap_group_map[object_id] = new_entry;
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
        sai_status_t status = installTrapClassify(
                object_id, entry, vpp_policer_index);

        if (status != SAI_STATUS_SUCCESS)
        {
            // create_internal() has already published the SAI object. Remove
            // it again so a retry does not see ITEM_ALREADY_EXISTS while the
            // trap is absent from both m_trap_map and the dataplane.
            sai_status_t rollback_status = remove_internal(
                    SAI_OBJECT_TYPE_HOSTIF_TRAP, sid);

            if (rollback_status != SAI_STATUS_SUCCESS)
            {
                SWSS_LOG_ERROR("failed to roll back hostif trap %s after VPP "
                        "bind failure: status %d", sid.c_str(), rollback_status);
                return rollback_status;
            }

            return status;
        }

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

    bool need_install = false;
    bool need_uninstall = false;
    bool tracked = false;
    bool should_be_installed = false;
    uint32_t old_vpp_policer_index = (uint32_t)~0;
    uint32_t new_vpp_policer_index = (uint32_t)~0;
    sai_object_id_t old_policer_oid = SAI_NULL_OBJECT_ID;
    sai_object_id_t new_policer_oid = SAI_NULL_OBJECT_ID;
    sai_object_id_t old_effective_policer_oid = SAI_NULL_OBJECT_ID;
    uint32_t old_effective_vpp_policer_index = (uint32_t)~0;
    bool old_binding_known = false;
    vpp_trap_entry_t old_entry{};
    vpp_trap_entry_t new_entry{};

    {
        std::lock_guard<std::mutex> lock(m_copp_state_mutex);

        auto it = m_trap_map.find(object_id);

        if (it != m_trap_map.end())
        {
            tracked = true;
            old_entry = it->second;
            new_entry = old_entry;

            switch (attr->id)
            {
                case SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION:
                    new_entry.packet_action = (sai_packet_action_t)attr->value.s32;
                    break;

                case SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP:
                    new_entry.trap_group_oid = attr->value.oid;
                    break;

                default:
                    break;
            }

            should_be_installed =
                (new_entry.packet_action == SAI_PACKET_ACTION_TRAP ||
                 new_entry.packet_action == SAI_PACKET_ACTION_COPY);

            need_install = should_be_installed &&
                (!old_entry.classify_installed ||
                 attr->id == SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP);
            need_uninstall = !should_be_installed && old_entry.classify_installed;

            auto resolve_policer = [&](const vpp_trap_entry_t &entry,
                    sai_object_id_t &policer_oid, uint32_t &vpp_index)
            {
                auto git = m_trap_group_map.find(entry.trap_group_oid);
                if (git == m_trap_group_map.end())
                {
                    return;
                }

                policer_oid = git->second.policer_oid;
                auto pit = m_policer_map.find(policer_oid);
                if (pit != m_policer_map.end())
                {
                    vpp_index = pit->second.vpp_policer_index;
                }
            };

            resolve_policer(old_entry, old_policer_oid,
                    old_vpp_policer_index);
            resolve_policer(new_entry, new_policer_oid,
                    new_vpp_policer_index);

            uint32_t key = 0;
            if (trapBindingKey(old_entry.trap_type, key))
            {
                old_binding_known = true;
                auto bit = m_trap_binding_map.find(key);
                if (bit != m_trap_binding_map.end())
                {
                    old_effective_policer_oid = bit->second;
                    auto pit = m_policer_map.find(old_effective_policer_oid);
                    if (pit != m_policer_map.end())
                    {
                        old_effective_vpp_policer_index =
                            pit->second.vpp_policer_index;
                    }
                }
            }
        }
    }

    if (!tracked)
    {
        SWSS_LOG_ERROR("hostif trap 0x%lx not tracked in m_trap_map; "
                "updating only internal state", (unsigned long)object_id);
        return set_internal(SAI_OBJECT_TYPE_HOSTIF_TRAP, serializedObjectId, attr);
    }

    sai_status_t status = SAI_STATUS_SUCCESS;

    if (need_install)
    {
        status = installTrapClassifyNow(object_id, new_entry,
                new_vpp_policer_index, new_policer_oid);
    }
    else if (need_uninstall)
    {
        status = uninstallTrapClassify(object_id, old_entry);
    }

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    status = set_internal(SAI_OBJECT_TYPE_HOSTIF_TRAP, serializedObjectId, attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        sai_status_t rollback_status = SAI_STATUS_SUCCESS;

        if ((need_install || need_uninstall) && old_binding_known)
        {
            rollback_status = programTrapClassifyNow(
                    object_id, old_entry,
                    old_effective_vpp_policer_index,
                    old_effective_policer_oid,
                    SAI_NULL_OBJECT_ID, false);
        }
        else if (need_install)
        {
            rollback_status = old_entry.classify_installed ?
                installTrapClassifyNow(object_id, old_entry,
                        old_vpp_policer_index, old_policer_oid) :
                uninstallTrapClassifyNow(object_id, new_entry);
        }
        else if (need_uninstall)
        {
            rollback_status = installTrapClassifyNow(object_id, old_entry,
                    old_vpp_policer_index, old_policer_oid);
        }

        if (rollback_status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("failed to restore trap 0x%lx after internal set "
                    "failure: status %d", (unsigned long)object_id,
                    rollback_status);
            return rollback_status;
        }

        return status;
    }

    new_entry.classify_installed = should_be_installed;
    {
        std::lock_guard<std::mutex> lock(m_copp_state_mutex);
        m_trap_map[object_id] = new_entry;
    }

    return SAI_STATUS_SUCCESS;
}
