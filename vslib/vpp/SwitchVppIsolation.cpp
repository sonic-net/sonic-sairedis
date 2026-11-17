/*
 * SAI isolation groups on VPP.
 *
 * SAI: a frame received on a port or bridge port whose
 * SAI_PORT_ATTR_ISOLATION_GROUP / SAI_BRIDGE_PORT_ATTR_ISOLATION_GROUP is a
 * group is not sent out of any member (SAI_ISOLATION_GROUP_MEMBER) of that
 * group. MC-LAG and EVPN split horizon are the users: mclagsyncd binds the
 * peer link to a group whose members are the dual-homed port-channels and EVPN
 * tunnel bridge ports, and ShlOrch binds each EVPN tunnel bridge port to a
 * group of multi-homed local ports.
 *
 * VPP's split-horizon groups can't carry this, since they are symmetric. The
 * sonic_ext plugin's isolation-group feature can: it is the SAI model on the
 * VPP interfaces of the L2 path. It works on bridged traffic only, which is
 * all an isolation group restricts in practice: routed traffic leaves through
 * a router interface, not a bridge port.
 *
 * A SAI port, LAG or port/LAG bridge port is a different VPP interface per
 * VLAN it is a member of (the parent for an untagged membership,
 * <parent>.<vid> for a tagged one), while a tunnel bridge port maps to the VPP
 * L2 VXLAN tunnel sw_if_index of each VNI on its SAI P2P tunnel. A binding or
 * membership is applied to each of those, and again whenever the bridge port
 * joins another VLAN or another L2 VXLAN tunnel is created for its tunnel. A
 * VPP sub-interface or VXLAN tunnel deleted with its membership leaves its
 * groups in sonic_ext by itself.
 *
 * Each SAI group derives its sonic_ext group id deterministically from its SAI
 * object index so warm-boot state restoration recovers the exact same VPP
 * group ids without a separate counter.
 */

#include "SwitchVpp.h"
#include "SwitchVppUtils.h"

#include <algorithm>
#include <set>

#include "meta/sai_serialize.h"
#include "vppxlate/SaiVppXlate.h"

#include "swss/logger.h"

using namespace saivs;

static constexpr uint32_t NO_ISOLATION_GROUP = ~0u;

static uint32_t vpp_id_for_isolation_group_oid(
        _In_ sai_object_id_t group_oid)
{
    SWSS_LOG_ENTER();

    /* RealObjectIdManager stores a unique 31-bit object index in bits 30..0. */
    return static_cast<uint32_t>(group_oid & 0x7fffffffULL) + 1u;
}

void SwitchVpp::restoreIsolationGroups()
{
    SWSS_LOG_ENTER();

    m_isolation_group_ids.clear();

    auto it = m_objectHash.find(SAI_OBJECT_TYPE_ISOLATION_GROUP);

    if (it == m_objectHash.end())
    {
        return;
    }

    for (const auto &kv: it->second)
    {
        sai_object_id_t group_oid = SAI_NULL_OBJECT_ID;
        sai_deserialize_object_id(kv.first, group_oid);

        if (group_oid != SAI_NULL_OBJECT_ID)
        {
            m_isolation_group_ids[group_oid] = vpp_id_for_isolation_group_oid(group_oid);
        }
    }
}

uint32_t SwitchVpp::isolation_group_vpp_id(
        _In_ sai_object_id_t group_oid)
{
    SWSS_LOG_ENTER();

    auto it = m_isolation_group_ids.find(group_oid);

    return it == m_isolation_group_ids.end() ? NO_ISOLATION_GROUP : it->second;
}

/*
 * The VPP interfaces (by name for port/LAG VLAN memberships, by sw_if_index
 * for L2 VXLAN tunnels) a port, LAG or bridge port bridges traffic on.
 */
void SwitchVpp::isolation_object_targets(
        _In_ sai_object_id_t object_id,
        _Out_ std::vector<std::string> &hwifs,
        _Out_ std::vector<uint32_t> &sw_ifs)
{
    SWSS_LOG_ENTER();

    hwifs.clear();
    sw_ifs.clear();

    sai_object_type_t type = objectTypeQuery(object_id);

    if (type == SAI_OBJECT_TYPE_BRIDGE_PORT)
    {
        if (is_tunnel_bridge_port(object_id))
        {
            auto tunnel = stored_attr(SAI_OBJECT_TYPE_BRIDGE_PORT,
                    sai_serialize_object_id(object_id), SAI_BRIDGE_PORT_ATTR_TUNNEL_ID);

            if (tunnel && tunnel->value.oid != SAI_NULL_OBJECT_ID)
            {
                m_tunnel_mgr.get_l2_vxlan_tunnel_if_indices(tunnel->value.oid, sw_ifs);
            }
            return;
        }

        bridge_port_vlan_hwifs(object_id, hwifs);
        return;
    }

    if (type != SAI_OBJECT_TYPE_PORT && type != SAI_OBJECT_TYPE_LAG)
    {
        return;
    }

    for (auto &kv: m_objectHash.at(SAI_OBJECT_TYPE_BRIDGE_PORT))
    {
        auto port = stored_attr(SAI_OBJECT_TYPE_BRIDGE_PORT, kv.first, SAI_BRIDGE_PORT_ATTR_PORT_ID);

        if (port == nullptr || port->value.oid != object_id)
        {
            continue;
        }

        sai_object_id_t br_port_id;
        sai_deserialize_object_id(kv.first, br_port_id);

        std::vector<std::string> br_hwifs;
        bridge_port_vlan_hwifs(br_port_id, br_hwifs);
        hwifs.insert(hwifs.end(), br_hwifs.begin(), br_hwifs.end());
    }
}

sai_status_t SwitchVpp::createIsolationGroup(
        _In_ sai_object_id_t group_oid,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    auto sid = sai_serialize_object_id(group_oid);

    bool enabled = false;

    CHECK_STATUS(sonicExtFeatureEnabled("isolation-group", enabled));

    if (!enabled)
    {
        SWSS_LOG_ERROR("Isolation group %s: VPP has no sonic-ext isolation-group feature", sid.c_str());
        return SAI_STATUS_NOT_SUPPORTED;
    }

    CHECK_STATUS(check_create_internal(SAI_OBJECT_TYPE_ISOLATION_GROUP, sid));

    uint32_t id = vpp_id_for_isolation_group_oid(group_oid);

    if (vpp_isolation_group_add_del(id, true) != 0)
    {
        SWSS_LOG_ERROR("Isolation group %s: VPP failed to create group %u", sid.c_str(), id);
        return SAI_STATUS_FAILURE;
    }

    sai_status_t status = create_internal(SAI_OBJECT_TYPE_ISOLATION_GROUP, sid, switch_id, attr_count, attr_list);

    if (status != SAI_STATUS_SUCCESS)
    {
        vpp_isolation_group_add_del(id, false);
        return status;
    }

    m_isolation_group_ids[group_oid] = id;

    SWSS_LOG_NOTICE("Isolation group %s is VPP group %u", sid.c_str(), id);

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::removeIsolationGroup(
        _In_ sai_object_id_t group_oid)
{
    SWSS_LOG_ENTER();

    auto sid = sai_serialize_object_id(group_oid);

    uint32_t id = isolation_group_vpp_id(group_oid);

    if (m_objectHash.at(SAI_OBJECT_TYPE_ISOLATION_GROUP).count(sid) == 0)
    {
        SWSS_LOG_ERROR("Isolation group %s does not exist", sid.c_str());
        return SAI_STATUS_ITEM_NOT_FOUND;
    }

    /* VPP refuses while the group has members or bindings, as SAI does. */
    if (id != NO_ISOLATION_GROUP && vpp_isolation_group_add_del(id, false) != 0)
    {
        SWSS_LOG_ERROR("Isolation group %s: VPP failed to delete group %u", sid.c_str(), id);
        return SAI_STATUS_OBJECT_IN_USE;
    }

    sai_status_t status = remove_internal(SAI_OBJECT_TYPE_ISOLATION_GROUP, sid);

    if (status != SAI_STATUS_SUCCESS)
    {
        if (id != NO_ISOLATION_GROUP && vpp_isolation_group_add_del(id, true) != 0)
        {
            SWSS_LOG_ERROR("Isolation group %s: VPP failed to recreate group %u", sid.c_str(), id);
        }
        return status;
    }

    m_isolation_group_ids.erase(group_oid);

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::vpp_isolation_member_add_del(
        _In_ uint32_t id,
        _In_ const std::vector<std::string> &hwifs,
        _In_ const std::vector<uint32_t> &sw_ifs,
        _In_ bool is_add)
{
    SWSS_LOG_ENTER();

    for (size_t i = 0; i < hwifs.size(); i++)
    {
        if (vpp_isolation_group_member_add_del(id, hwifs[i].c_str(), is_add) == 0)
        {
            continue;
        }

        SWSS_LOG_ERROR("VPP failed to %s %s %s isolation group %u",
                is_add ? "add" : "remove", hwifs[i].c_str(), is_add ? "to" : "from", id);

        while (i-- > 0)
        {
            vpp_isolation_group_member_add_del(id, hwifs[i].c_str(), !is_add);
        }
        return SAI_STATUS_FAILURE;
    }

    for (size_t k = 0; k < sw_ifs.size(); k++)
    {
        if (vpp_isolation_group_member_add_del_by_index(id, sw_ifs[k], is_add) == 0)
        {
            continue;
        }

        SWSS_LOG_ERROR("VPP failed to %s sw_if %u %s isolation group %u",
                is_add ? "add" : "remove", sw_ifs[k], is_add ? "to" : "from", id);

        while (k-- > 0)
        {
            vpp_isolation_group_member_add_del_by_index(id, sw_ifs[k], !is_add);
        }
        for (size_t i = hwifs.size(); i-- > 0;)
        {
            vpp_isolation_group_member_add_del(id, hwifs[i].c_str(), !is_add);
        }
        return SAI_STATUS_FAILURE;
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::createIsolationGroupMember(
        _In_ sai_object_id_t member_oid,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    auto sid = sai_serialize_object_id(member_oid);

    auto group = sai_metadata_get_attr_by_id(SAI_ISOLATION_GROUP_MEMBER_ATTR_ISOLATION_GROUP_ID, attr_count, attr_list);
    auto object = sai_metadata_get_attr_by_id(SAI_ISOLATION_GROUP_MEMBER_ATTR_ISOLATION_OBJECT, attr_count, attr_list);

    if (group == nullptr || object == nullptr)
    {
        SWSS_LOG_ERROR("Isolation group member %s: group or object missing", sid.c_str());
        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    uint32_t id = isolation_group_vpp_id(group->value.oid);

    if (id == NO_ISOLATION_GROUP)
    {
        SWSS_LOG_ERROR("Isolation group member %s: unknown group %s", sid.c_str(),
                sai_serialize_object_id(group->value.oid).c_str());
        return SAI_STATUS_INVALID_PARAMETER;
    }

    CHECK_STATUS(check_create_internal(SAI_OBJECT_TYPE_ISOLATION_GROUP_MEMBER, sid));

    std::vector<std::string> hwifs;
    std::vector<uint32_t> sw_ifs;
    isolation_object_targets(object->value.oid, hwifs, sw_ifs);

    CHECK_STATUS(vpp_isolation_member_add_del(id, hwifs, sw_ifs, true));

    sai_status_t status = create_internal(SAI_OBJECT_TYPE_ISOLATION_GROUP_MEMBER, sid, switch_id, attr_count, attr_list);

    if (status != SAI_STATUS_SUCCESS)
    {
        vpp_isolation_member_add_del(id, hwifs, sw_ifs, false);
    }

    return status;
}

sai_status_t SwitchVpp::removeIsolationGroupMember(
        _In_ sai_object_id_t member_oid)
{
    SWSS_LOG_ENTER();

    auto sid = sai_serialize_object_id(member_oid);

    auto group = stored_attr(SAI_OBJECT_TYPE_ISOLATION_GROUP_MEMBER, sid, SAI_ISOLATION_GROUP_MEMBER_ATTR_ISOLATION_GROUP_ID);
    auto object = stored_attr(SAI_OBJECT_TYPE_ISOLATION_GROUP_MEMBER, sid, SAI_ISOLATION_GROUP_MEMBER_ATTR_ISOLATION_OBJECT);

    if (group && object)
    {
        uint32_t id = isolation_group_vpp_id(group->value.oid);

        std::vector<std::string> hwifs;
        std::vector<uint32_t> sw_ifs;
        isolation_object_targets(object->value.oid, hwifs, sw_ifs);

        if (id != NO_ISOLATION_GROUP)
        {
            CHECK_STATUS(vpp_isolation_member_add_del(id, hwifs, sw_ifs, false));
        }
    }

    return remove_internal(SAI_OBJECT_TYPE_ISOLATION_GROUP_MEMBER, sid);
}

/*
 * SAI_PORT_ATTR_ISOLATION_GROUP and SAI_BRIDGE_PORT_ATTR_ISOLATION_GROUP:
 * bind the frames received on the object to a group, or unbind them.
 */
sai_status_t SwitchVpp::setIsolationGroupBinding(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    auto sid = sai_serialize_object_id(object_id);

    uint32_t id = NO_ISOLATION_GROUP;

    if (attr->value.oid != SAI_NULL_OBJECT_ID)
    {
        id = isolation_group_vpp_id(attr->value.oid);

        if (id == NO_ISOLATION_GROUP)
        {
            SWSS_LOG_ERROR("%s: unknown isolation group %s", sid.c_str(),
                    sai_serialize_object_id(attr->value.oid).c_str());
            return SAI_STATUS_INVALID_ATTR_VALUE_0;
        }
    }

    uint32_t old_id = NO_ISOLATION_GROUP;

    auto prev = stored_attr(object_type, sid, attr->id);

    if (prev && prev->value.oid != SAI_NULL_OBJECT_ID)
    {
        old_id = isolation_group_vpp_id(prev->value.oid);
    }

    std::vector<std::string> hwifs;
    std::vector<uint32_t> sw_ifs;
    isolation_object_targets(object_id, hwifs, sw_ifs);

    for (size_t i = 0; i < hwifs.size(); ++i)
    {
        if (vpp_isolation_group_bind(hwifs[i].c_str(), id) != 0)
        {
            SWSS_LOG_ERROR("VPP failed to bind %s to isolation group %u", hwifs[i].c_str(), id);
            for (size_t j = 0; j < i; ++j)
            {
                vpp_isolation_group_bind(hwifs[j].c_str(), old_id);
            }
            return SAI_STATUS_FAILURE;
        }
    }

    for (size_t k = 0; k < sw_ifs.size(); ++k)
    {
        if (vpp_isolation_group_bind_by_index(sw_ifs[k], id) != 0)
        {
            SWSS_LOG_ERROR("VPP failed to bind sw_if %u to isolation group %u", sw_ifs[k], id);
            for (size_t j = 0; j < k; ++j)
            {
                vpp_isolation_group_bind_by_index(sw_ifs[j], old_id);
            }
            for (auto &hwif: hwifs)
            {
                vpp_isolation_group_bind(hwif.c_str(), old_id);
            }
            return SAI_STATUS_FAILURE;
        }
    }

    sai_status_t status = set_internal(object_type, sid, attr);

    if (status != SAI_STATUS_SUCCESS)
    {
        for (uint32_t sw_if: sw_ifs)
        {
            vpp_isolation_group_bind_by_index(sw_if, old_id);
        }
        for (auto &hwif: hwifs)
        {
            vpp_isolation_group_bind(hwif.c_str(), old_id);
        }
        return status;
    }

    SWSS_LOG_NOTICE("%s bound to isolation group %u on %zu interfaces and %zu tunnels",
            sid.c_str(), id, hwifs.size(), sw_ifs.size());

    return SAI_STATUS_SUCCESS;
}

/*
 * A bridge port joined a VLAN through hwif, a VPP interface no isolation
 * group knows yet: bind it and add it to its groups as its port or bridge
 * port is.
 */
sai_status_t SwitchVpp::vpp_isolation_vlan_member_added(
        _In_ sai_object_id_t br_port_id,
        _In_ const std::string &hwif)
{
    SWSS_LOG_ENTER();

    if (m_isolation_group_ids.empty())
    {
        return SAI_STATUS_SUCCESS;
    }

    auto br_sid = sai_serialize_object_id(br_port_id);

    auto port = stored_attr(SAI_OBJECT_TYPE_BRIDGE_PORT, br_sid, SAI_BRIDGE_PORT_ATTR_PORT_ID);

    sai_object_id_t port_id = port ? port->value.oid : SAI_NULL_OBJECT_ID;

    auto binding = stored_attr(SAI_OBJECT_TYPE_BRIDGE_PORT, br_sid, SAI_BRIDGE_PORT_ATTR_ISOLATION_GROUP);

    if ((binding == nullptr || binding->value.oid == SAI_NULL_OBJECT_ID) &&
        objectTypeQuery(port_id) == SAI_OBJECT_TYPE_PORT)
    {
        binding = stored_attr(SAI_OBJECT_TYPE_PORT, sai_serialize_object_id(port_id), SAI_PORT_ATTR_ISOLATION_GROUP);
    }

    bool bound = false;

    if (binding && binding->value.oid != SAI_NULL_OBJECT_ID)
    {
        uint32_t id = isolation_group_vpp_id(binding->value.oid);

        if (id != NO_ISOLATION_GROUP)
        {
            if (vpp_isolation_group_bind(hwif.c_str(), id) != 0)
            {
                SWSS_LOG_ERROR("VPP failed to bind %s to isolation group %u", hwif.c_str(), id);
                return SAI_STATUS_FAILURE;
            }
            bound = true;
        }
    }

    std::vector<uint32_t> added_groups;

    for (auto &kv: m_objectHash.at(SAI_OBJECT_TYPE_ISOLATION_GROUP_MEMBER))
    {
        auto object = stored_attr(SAI_OBJECT_TYPE_ISOLATION_GROUP_MEMBER, kv.first, SAI_ISOLATION_GROUP_MEMBER_ATTR_ISOLATION_OBJECT);
        auto group = stored_attr(SAI_OBJECT_TYPE_ISOLATION_GROUP_MEMBER, kv.first, SAI_ISOLATION_GROUP_MEMBER_ATTR_ISOLATION_GROUP_ID);

        if (object == nullptr || group == nullptr ||
            (object->value.oid != br_port_id && object->value.oid != port_id))
        {
            continue;
        }

        uint32_t id = isolation_group_vpp_id(group->value.oid);

        if (id == NO_ISOLATION_GROUP)
        {
            continue;
        }

        if (vpp_isolation_group_member_add_del(id, hwif.c_str(), true) != 0)
        {
            SWSS_LOG_ERROR("VPP failed to add %s to isolation group %u", hwif.c_str(), id);
            for (uint32_t gid: added_groups)
            {
                vpp_isolation_group_member_add_del(gid, hwif.c_str(), false);
            }
            if (bound)
            {
                vpp_isolation_group_bind(hwif.c_str(), NO_ISOLATION_GROUP);
            }
            return SAI_STATUS_FAILURE;
        }
        added_groups.push_back(id);
    }

    return SAI_STATUS_SUCCESS;
}

/*
 * An EVPN L2 VXLAN tunnel (sw_if_index) was created or attached for tunnel_oid:
 * apply any isolation-group binding or memberships of tunnel bridge ports
 * referencing tunnel_oid. other_tunnels already share sw_if_index; VPP binds
 * an interface to one group, so their binding stands and a different one for
 * tunnel_oid is refused.
 */
sai_status_t SwitchVpp::vpp_isolation_tunnel_added(
        _In_ sai_object_id_t tunnel_oid,
        _In_ uint32_t sw_if_index,
        _In_ const std::set<sai_object_id_t> &other_tunnels)
{
    SWSS_LOG_ENTER();

    if (m_isolation_group_ids.empty())
    {
        return SAI_STATUS_SUCCESS;
    }

    std::set<sai_object_id_t> tunnel_bps;
    std::set<sai_object_id_t> other_bps;

    for (auto &kv: m_objectHash.at(SAI_OBJECT_TYPE_BRIDGE_PORT))
    {
        auto type = stored_attr(SAI_OBJECT_TYPE_BRIDGE_PORT, kv.first, SAI_BRIDGE_PORT_ATTR_TYPE);
        auto tunnel = stored_attr(SAI_OBJECT_TYPE_BRIDGE_PORT, kv.first, SAI_BRIDGE_PORT_ATTR_TUNNEL_ID);

        if (!type || type->value.s32 != SAI_BRIDGE_PORT_TYPE_TUNNEL || !tunnel)
        {
            continue;
        }

        sai_object_id_t bp_oid = SAI_NULL_OBJECT_ID;
        sai_deserialize_object_id(kv.first, bp_oid);

        if (tunnel->value.oid == tunnel_oid)
        {
            tunnel_bps.insert(bp_oid);
        }
        else if (other_tunnels.count(tunnel->value.oid) > 0)
        {
            other_bps.insert(bp_oid);
        }
    }

    if (tunnel_bps.empty())
    {
        return SAI_STATUS_SUCCESS;
    }

    auto bound_group = [&](const std::set<sai_object_id_t> &bps) {
        for (sai_object_id_t bp_oid: bps)
        {
            auto binding = stored_attr(SAI_OBJECT_TYPE_BRIDGE_PORT,
                    sai_serialize_object_id(bp_oid), SAI_BRIDGE_PORT_ATTR_ISOLATION_GROUP);

            if (binding && binding->value.oid != SAI_NULL_OBJECT_ID)
            {
                uint32_t id = isolation_group_vpp_id(binding->value.oid);
                if (id != NO_ISOLATION_GROUP)
                {
                    return id;
                }
            }
        }
        return NO_ISOLATION_GROUP;
    };

    uint32_t bind_id = bound_group(tunnel_bps);
    uint32_t other_bind_id = bound_group(other_bps);

    if (bind_id != NO_ISOLATION_GROUP && other_bind_id != NO_ISOLATION_GROUP && bind_id != other_bind_id)
    {
        SWSS_LOG_ERROR("Tunnel sw_if %u is bound to isolation group %u by another owner, cannot bind it to %u",
                sw_if_index, other_bind_id, bind_id);
        return SAI_STATUS_NOT_SUPPORTED;
    }

    bool bound = false;

    if (bind_id != NO_ISOLATION_GROUP && other_bind_id == NO_ISOLATION_GROUP)
    {
        if (vpp_isolation_group_bind_by_index(sw_if_index, bind_id) != 0)
        {
            SWSS_LOG_ERROR("VPP failed to bind tunnel sw_if %u to isolation group %u", sw_if_index, bind_id);
            return SAI_STATUS_FAILURE;
        }
        bound = true;
    }

    std::set<uint32_t> held_groups;

    for (auto &kv: m_objectHash.at(SAI_OBJECT_TYPE_ISOLATION_GROUP_MEMBER))
    {
        auto object = stored_attr(SAI_OBJECT_TYPE_ISOLATION_GROUP_MEMBER, kv.first, SAI_ISOLATION_GROUP_MEMBER_ATTR_ISOLATION_OBJECT);
        auto group = stored_attr(SAI_OBJECT_TYPE_ISOLATION_GROUP_MEMBER, kv.first, SAI_ISOLATION_GROUP_MEMBER_ATTR_ISOLATION_GROUP_ID);

        if (object && group && other_bps.count(object->value.oid) > 0)
        {
            held_groups.insert(isolation_group_vpp_id(group->value.oid));
        }
    }

    std::vector<uint32_t> added_groups;

    for (auto &kv: m_objectHash.at(SAI_OBJECT_TYPE_ISOLATION_GROUP_MEMBER))
    {
        auto object = stored_attr(SAI_OBJECT_TYPE_ISOLATION_GROUP_MEMBER, kv.first, SAI_ISOLATION_GROUP_MEMBER_ATTR_ISOLATION_OBJECT);
        auto group = stored_attr(SAI_OBJECT_TYPE_ISOLATION_GROUP_MEMBER, kv.first, SAI_ISOLATION_GROUP_MEMBER_ATTR_ISOLATION_GROUP_ID);

        if (object == nullptr || group == nullptr || tunnel_bps.count(object->value.oid) == 0)
        {
            continue;
        }

        uint32_t id = isolation_group_vpp_id(group->value.oid);

        if (id == NO_ISOLATION_GROUP || held_groups.count(id) > 0 ||
            std::find(added_groups.begin(), added_groups.end(), id) != added_groups.end())
        {
            continue;
        }

        if (vpp_isolation_group_member_add_del_by_index(id, sw_if_index, true) != 0)
        {
            SWSS_LOG_ERROR("VPP failed to add tunnel sw_if %u to isolation group %u", sw_if_index, id);
            for (uint32_t gid: added_groups)
            {
                vpp_isolation_group_member_add_del_by_index(gid, sw_if_index, false);
            }
            if (bound)
            {
                vpp_isolation_group_bind_by_index(sw_if_index, NO_ISOLATION_GROUP);
            }
            return SAI_STATUS_FAILURE;
        }
        added_groups.push_back(id);
    }

    return SAI_STATUS_SUCCESS;
}

void SwitchVpp::vpp_isolation_tunnel_released(
        _In_ sai_object_id_t tunnel_oid,
        _In_ uint32_t sw_if_index,
        _In_ const std::set<sai_object_id_t> &remaining_tunnels)
{
    SWSS_LOG_ENTER();

    if (m_isolation_group_ids.empty())
    {
        return;
    }

    std::set<sai_object_id_t> released_bps;
    std::set<sai_object_id_t> remaining_bps;

    for (auto &kv: m_objectHash.at(SAI_OBJECT_TYPE_BRIDGE_PORT))
    {
        auto type = stored_attr(SAI_OBJECT_TYPE_BRIDGE_PORT, kv.first, SAI_BRIDGE_PORT_ATTR_TYPE);
        auto tunnel = stored_attr(SAI_OBJECT_TYPE_BRIDGE_PORT, kv.first, SAI_BRIDGE_PORT_ATTR_TUNNEL_ID);

        if (!type || type->value.s32 != SAI_BRIDGE_PORT_TYPE_TUNNEL || !tunnel)
        {
            continue;
        }

        sai_object_id_t bp_oid = SAI_NULL_OBJECT_ID;
        sai_deserialize_object_id(kv.first, bp_oid);

        if (tunnel->value.oid == tunnel_oid)
        {
            released_bps.insert(bp_oid);
        }
        else if (remaining_tunnels.count(tunnel->value.oid) > 0)
        {
            remaining_bps.insert(bp_oid);
        }
    }

    if (released_bps.empty())
    {
        return;
    }

    uint32_t keep_bind = NO_ISOLATION_GROUP;
    for (sai_object_id_t bp_oid: remaining_bps)
    {
        auto binding = stored_attr(SAI_OBJECT_TYPE_BRIDGE_PORT,
                sai_serialize_object_id(bp_oid), SAI_BRIDGE_PORT_ATTR_ISOLATION_GROUP);
        if (binding && binding->value.oid != SAI_NULL_OBJECT_ID)
        {
            keep_bind = isolation_group_vpp_id(binding->value.oid);
            if (keep_bind != NO_ISOLATION_GROUP)
            {
                break;
            }
        }
    }

    for (sai_object_id_t bp_oid: released_bps)
    {
        auto binding = stored_attr(SAI_OBJECT_TYPE_BRIDGE_PORT,
                sai_serialize_object_id(bp_oid), SAI_BRIDGE_PORT_ATTR_ISOLATION_GROUP);
        if (binding && binding->value.oid != SAI_NULL_OBJECT_ID)
        {
            if (vpp_isolation_group_bind_by_index(sw_if_index, keep_bind) != 0)
            {
                SWSS_LOG_ERROR("VPP failed to bind sw_if %u to isolation group %u on tunnel leave",
                        sw_if_index, keep_bind);
            }
            break;
        }
    }

    std::set<uint32_t> keep_groups;
    std::set<uint32_t> drop_groups;

    for (auto &kv: m_objectHash.at(SAI_OBJECT_TYPE_ISOLATION_GROUP_MEMBER))
    {
        auto object = stored_attr(SAI_OBJECT_TYPE_ISOLATION_GROUP_MEMBER, kv.first, SAI_ISOLATION_GROUP_MEMBER_ATTR_ISOLATION_OBJECT);
        auto group = stored_attr(SAI_OBJECT_TYPE_ISOLATION_GROUP_MEMBER, kv.first, SAI_ISOLATION_GROUP_MEMBER_ATTR_ISOLATION_GROUP_ID);

        if (!object || !group)
        {
            continue;
        }

        uint32_t id = isolation_group_vpp_id(group->value.oid);
        if (id == NO_ISOLATION_GROUP)
        {
            continue;
        }

        if (remaining_bps.count(object->value.oid) > 0)
        {
            keep_groups.insert(id);
        }
        else if (released_bps.count(object->value.oid) > 0)
        {
            drop_groups.insert(id);
        }
    }

    for (uint32_t gid: drop_groups)
    {
        if (keep_groups.count(gid) == 0)
        {
            if (vpp_isolation_group_member_add_del_by_index(gid, sw_if_index, false) != 0)
            {
                SWSS_LOG_ERROR("VPP failed to remove sw_if %u from isolation group %u on tunnel leave",
                        sw_if_index, gid);
            }
        }
    }
}
