#include "SwitchVpp.h"
#include "SwitchVppNexthop.h"
#include "SwitchVppUtils.h"

#include "meta/sai_serialize.h"
#include "meta/NotificationPortStateChange.h"

#include "swss/logger.h"
#include "swss/exec.h"
#include "swss/converter.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdint.h>
#include <vector>

#include "vppxlate/SaiVppXlate.h"

using namespace saivs;

#define CHECK_STATUS_QUIET(status) {                        \
    sai_status_t _status = (status);                        \
    if (_status != SAI_STATUS_SUCCESS) { return _status; } }

#define CHECK_STATUS_W_MSG(status, msg, ...) {                                  \
    sai_status_t _status = (status);                            \
    if (_status != SAI_STATUS_SUCCESS) { \
        char buffer[512]; \
        snprintf(buffer, 512, msg, ##__VA_ARGS__); \
        SWSS_LOG_ERROR("%s: status %d", buffer, status); \
        return _status; } }

void create_route_prefix_entry (
       sai_route_entry_t *route_entry,
       vpp_ip_route_t *ip_route)
{
    SWSS_LOG_ENTER();

    const sai_ip_prefix_t *ip_address = &route_entry->destination;

    switch (ip_address->addr_family) {
    case SAI_IP_ADDR_FAMILY_IPV4:
    {
        struct sockaddr_in *sin =  &ip_route->prefix_addr.addr.ip4;

        ip_route->prefix_addr.sa_family = AF_INET;
        sin->sin_addr.s_addr = ip_address->addr.ip4;
        ip_route->prefix_len = getPrefixLenFromAddrMask(reinterpret_cast<const uint8_t*>(&ip_address->mask.ip4), 4);

        break;
    }
    case SAI_IP_ADDR_FAMILY_IPV6:
    {
        struct sockaddr_in6 *sin6 =  &ip_route->prefix_addr.addr.ip6;

        ip_route->prefix_addr.sa_family = AF_INET6;
        memcpy(sin6->sin6_addr.s6_addr, ip_address->addr.ip6, sizeof(sin6->sin6_addr.s6_addr));
        ip_route->prefix_len = getPrefixLenFromAddrMask(ip_address->mask.ip6, 16);

        break;
    }
    }
}

void create_vpp_nexthop_entry (
    nexthop_grp_member_t *nxt_grp_member,
    const char *hwif_name,
    vpp_nexthop_type_e type,
    vpp_ip_nexthop_t *vpp_nexthop)
{
    SWSS_LOG_ENTER();

    sai_ip_address_t *ip_address = &nxt_grp_member->addr;

    switch (ip_address->addr_family) {
    case SAI_IP_ADDR_FAMILY_IPV4:
    {
        struct sockaddr_in *sin =  &vpp_nexthop->addr.addr.ip4;

        vpp_nexthop->addr.sa_family = AF_INET;
        sin->sin_addr.s_addr = ip_address->addr.ip4;

        break;
    }
    case SAI_IP_ADDR_FAMILY_IPV6:
    {
        struct sockaddr_in6 *sin6 =  &vpp_nexthop->addr.addr.ip6;

        vpp_nexthop->addr.sa_family = AF_INET6;
        memcpy(sin6->sin6_addr.s6_addr, ip_address->addr.ip6, sizeof(sin6->sin6_addr.s6_addr));

        break;
    }
    }
    vpp_nexthop->type = type;
    vpp_nexthop->hwif_name = hwif_name;
    vpp_nexthop->sw_if_index = nxt_grp_member->sw_if_index;
    vpp_nexthop->weight = (uint8_t) nxt_grp_member->weight;
    vpp_nexthop->preference = 0;
}

sai_status_t SwitchVpp::IpRouteAddRemove(
        _In_ const SaiObject* route_obj,
        _In_ bool is_add,
        _Out_ uint32_t *stats_index)
{
    SWSS_LOG_ENTER();

    int ret = SAI_STATUS_SUCCESS;
    sai_status_t                 status;
    sai_object_id_t              next_hop_oid;
    sai_attribute_t              attr;
    std::string                  serializedObjectId = route_obj->get_id();
    int                          packet_action = SAI_PACKET_ACTION_FORWARD;

    attr.id = SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION;
    status = route_obj->get_attr(attr);
    if (status == SAI_STATUS_SUCCESS) {
        packet_action = attr.value.s32;
    }

    // We should program drop routes
    if (packet_action != SAI_PACKET_ACTION_FORWARD) {
        SWSS_LOG_NOTICE("Ignoring ip route %s: action is not forward: %d",
                        serializedObjectId.c_str(), packet_action);
        return SAI_STATUS_SUCCESS;
    }

    attr.id = SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID;
    status = route_obj->get_attr(attr);
    if (status != SAI_STATUS_SUCCESS) {
        SWSS_LOG_NOTICE("Ignoring ip route %s: no next hop id",
                        serializedObjectId.c_str());
        return SAI_STATUS_SUCCESS;
    }
    next_hop_oid = attr.value.oid;

    sai_route_entry_t route_entry;
    const char *hwif_name = NULL;
    vpp_nexthop_type_e nexthop_type = VPP_NEXTHOP_NORMAL;
    bool config_ip_route = false;

    sai_deserialize_route_entry(serializedObjectId, route_entry);

    nexthop_grp_config_t *nxthop_group = NULL;

    if (SAI_OBJECT_TYPE_ROUTER_INTERFACE == RealObjectIdManager::objectTypeQuery(next_hop_oid))
    {
        sai_status_t rif_status = vpp_add_del_intf_ip_addr(route_entry.destination, next_hop_oid, is_add);
        if (rif_status != SAI_STATUS_SUCCESS)
        {
            return rif_status;
        }
    }
    else if (SAI_OBJECT_TYPE_PORT == RealObjectIdManager::objectTypeQuery(next_hop_oid))
    {
        attr.id = SAI_ROUTE_ENTRY_ATTR_PACKET_ACTION;
        status = route_obj->get_attr(attr);

        if (status == SAI_STATUS_SUCCESS && SAI_PACKET_ACTION_FORWARD == attr.value.s32) {
            vpp_add_del_intf_ip_addr_norif(serializedObjectId, route_entry, is_add);
        }
    }
    else if (SAI_OBJECT_TYPE_NEXT_HOP == RealObjectIdManager::objectTypeQuery(next_hop_oid))
    {
        if (IpRouteNexthopEntry(next_hop_oid, &nxthop_group) == SAI_STATUS_SUCCESS)
        {
            config_ip_route = true;
        }
    }
    else if (SAI_OBJECT_TYPE_NEXT_HOP_GROUP == RealObjectIdManager::objectTypeQuery(next_hop_oid))
    {
        if (IpRouteNexthopGroupEntry(next_hop_oid, &nxthop_group) == SAI_STATUS_SUCCESS)
        {
            config_ip_route = true;
        }
    }

    if (config_ip_route == true)
    {
        std::shared_ptr<IpVrfInfo> vrf;
        uint32_t vrf_id;

        vrf = vpp_get_ip_vrf(route_entry.vr_id);
        if (vrf == nullptr) {
            vrf_id = 0;
        } else {
            vrf_id = vrf->m_vrf_id;
        }
        vpp_ip_route_t *ip_route = (vpp_ip_route_t *)
            calloc(1, sizeof(vpp_ip_route_t) + (nxthop_group->nmembers * sizeof(vpp_ip_nexthop_t)));
        if (!ip_route) {
            return SAI_STATUS_FAILURE;
        }
        create_route_prefix_entry(&route_entry, ip_route);
        ip_route->vrf_id = vrf_id;
        ip_route->is_multipath = (nxthop_group->nmembers > 1) ? true : false;

        nexthop_grp_member_t *nxt_grp_member;

        nxt_grp_member = nxthop_group->grp_members;

        std::vector<std::string> member_hwif_storage;
        member_hwif_storage.reserve(nxthop_group->nmembers);

        size_t i;
        for (i = 0; i < nxthop_group->nmembers; i++) {
            const char *member_hwif = hwif_name;
            std::string member_hwif_str;

            if (member_hwif == NULL && nxt_grp_member->rif_oid != SAI_NULL_OBJECT_ID)
            {
                sai_attribute_t rif_attr;
                uint16_t vlan_id = 0;

                rif_attr.id = SAI_ROUTER_INTERFACE_ATTR_TYPE;
                if (get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, nxt_grp_member->rif_oid, 1, &rif_attr) == SAI_STATUS_SUCCESS &&
                    rif_attr.value.s32 == SAI_ROUTER_INTERFACE_TYPE_SUB_PORT)
                {
                    sai_attribute_t vlan_attr;
                    vlan_attr.id = SAI_ROUTER_INTERFACE_ATTR_OUTER_VLAN_ID;

                    if (get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, nxt_grp_member->rif_oid, 1, &vlan_attr) == SAI_STATUS_SUCCESS)
                    {
                        vlan_id = vlan_attr.value.u16;
                    }
                }

                rif_attr.id = SAI_ROUTER_INTERFACE_ATTR_PORT_ID;
                if (get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, nxt_grp_member->rif_oid, 1, &rif_attr) == SAI_STATUS_SUCCESS &&
                    vpp_get_hwif_name(rif_attr.value.oid, vlan_id, member_hwif_str))
                {
                    member_hwif_storage.push_back(std::move(member_hwif_str));
                    member_hwif = member_hwif_storage.back().c_str();
                }
            }

            create_vpp_nexthop_entry(nxt_grp_member, member_hwif, nexthop_type,  &ip_route->nexthop[i]);
            nxt_grp_member++;
        }
        ip_route->nexthop_cnt = nxthop_group->nmembers;

        ret = ip_route_add_del_get_stats(ip_route, is_add, is_add ? stats_index : NULL);

        SWSS_LOG_NOTICE("%s ip route in VS %s status %d table %u", (is_add ? "Add" : "Remove"),
                        serializedObjectId.c_str(), ret, vrf_id);
        SWSS_LOG_NOTICE("%s route nexthop type %s count %u", (is_add ? "Add" : "Remove"),
                        sai_serialize_object_type(RealObjectIdManager::objectTypeQuery(next_hop_oid)).c_str(),
                        nxthop_group->nmembers);

        if (ret == 0 && is_add && stats_index && *stats_index == UINT32_MAX)
        {
            SWSS_LOG_WARN("VPP did not return a route stats index for %s", serializedObjectId.c_str());
        }

        free(ip_route);
        free(nxthop_group);

    } else {
        SWSS_LOG_NOTICE("Ignoring VS ip route %s", serializedObjectId.c_str());
    }

    return (ret == 0) ? SAI_STATUS_SUCCESS : SAI_STATUS_FAILURE;
}

sai_status_t SwitchVpp::setRouteCounterBinding(
        _In_ const std::string &serializedObjectId,
        _In_ sai_object_id_t counter_oid)
{
    SWSS_LOG_ENTER();

    if (counter_oid == SAI_NULL_OBJECT_ID)
    {
        removeRouteCounterBinding(serializedObjectId);
        return SAI_STATUS_SUCCESS;
    }

    // Capture the route's existing counter binding (read from the committed
    // SaiObjectDB relationship, before the caller's create_internal/set_internal
    // persists the new binding) so base/carry can be restored on rollback. The
    // create path is always unbound here. A same-counter re-set (A -> A) still
    // uses this for rollback if the new stats read fails. A direct rebind to a
    // *different* counter (A -> B) is SAI-legal but not expected from current
    // SONiC FlowCounterRouteOrch (which toggles NULL <-> counter); warn if seen.
    sai_object_id_t old_counter_oid = getRouteBoundCounter(serializedObjectId);
    std::map<sai_stat_id_t, uint64_t> old_counter_base;
    std::map<sai_stat_id_t, uint64_t> old_counter_carry;
    bool has_old_counter_base = false;
    bool has_old_counter_carry = false;
    if (old_counter_oid != SAI_NULL_OBJECT_ID)
    {
        if (old_counter_oid != counter_oid)
        {
            SWSS_LOG_WARN("unexpected direct rebind of route %s counter %s -> %s without intervening NULL",
                    serializedObjectId.c_str(),
                    sai_serialize_object_id(old_counter_oid).c_str(),
                    sai_serialize_object_id(counter_oid).c_str());
        }
        auto oldBaseIt = m_routeCounterStatsBaseMap.find(old_counter_oid);
        if (oldBaseIt != m_routeCounterStatsBaseMap.end())
        {
            old_counter_base = oldBaseIt->second;
            has_old_counter_base = true;
        }
        auto oldCarryIt = m_routeCounterStatsCarryMap.find(old_counter_oid);
        if (oldCarryIt != m_routeCounterStatsCarryMap.end())
        {
            old_counter_carry = oldCarryIt->second;
            has_old_counter_carry = true;
        }
    }

    // Enforce the 1:1 invariant: reject a counter already bound to a different
    // route (read from the committed relationship).
    std::string boundRoute;
    if (getCounterBoundRoute(counter_oid, boundRoute) &&
        boundRoute != serializedObjectId)
    {
        SWSS_LOG_ERROR("counter %s is already bound to route %s",
                sai_serialize_object_id(counter_oid).c_str(),
                boundRoute.c_str());
        return SAI_STATUS_OBJECT_IN_USE;
    }

    // Drop the old counter's base/carry (already captured above for rollback).
    if (old_counter_oid != SAI_NULL_OBJECT_ID)
    {
        m_routeCounterStatsBaseMap.erase(old_counter_oid);
        m_routeCounterStatsCarryMap.erase(old_counter_oid);
    }

    // Snapshot the new counter's base from the route's VPP stats index. The
    // route -> index mapping is already populated (callers guarantee a valid
    // stats index for a non-NULL counter), so this does not depend on the new
    // counter<->route relationship being committed yet.
    auto statsIt = m_routeStatsIndexMap.find(serializedObjectId);
    if (statsIt == m_routeStatsIndexMap.end())
    {
        SWSS_LOG_ERROR("missing VPP stats index for route %s binding counter %s",
                serializedObjectId.c_str(),
                sai_serialize_object_id(counter_oid).c_str());
        return SAI_STATUS_FAILURE;
    }

    std::map<sai_stat_id_t, uint64_t> stats;
    auto status = readRouteStatsByIndex(statsIt->second, stats);
    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("failed to read VPP route stats for route %s counter %s stats index %u",
                serializedObjectId.c_str(),
                sai_serialize_object_id(counter_oid).c_str(),
                statsIt->second);
        if (old_counter_oid != SAI_NULL_OBJECT_ID)
        {
            if (has_old_counter_base)
            {
                m_routeCounterStatsBaseMap[old_counter_oid] = old_counter_base;
            }
            if (has_old_counter_carry)
            {
                m_routeCounterStatsCarryMap[old_counter_oid] = old_counter_carry;
            }
        }
        return status;
    }

    m_routeCounterStatsBaseMap[counter_oid] = stats;
    m_routeCounterStatsCarryMap.erase(counter_oid);

    return SAI_STATUS_SUCCESS;
}

void SwitchVpp::removeRouteCounterBinding(
        _In_ const std::string &serializedObjectId)
{
    SWSS_LOG_ENTER();

    // Resolve the currently bound counter from the committed relationship; the
    // caller must invoke this while the route's COUNTER_ID is still persisted.
    sai_object_id_t counter_oid = getRouteBoundCounter(serializedObjectId);
    if (counter_oid == SAI_NULL_OBJECT_ID)
    {
        return;
    }

    m_routeCounterStatsBaseMap.erase(counter_oid);
    m_routeCounterStatsCarryMap.erase(counter_oid);
}

// Add or remove a single path from a multipath route
// Uses is_multipath=true to tell VPP to add/remove a path rather than replace the entire route
sai_status_t SwitchVpp::IpRoutePathAddRemove(
        _In_ const SaiObject* route_obj,
        _In_ nexthop_grp_member_t *member,
        _In_ bool is_add,
        _Out_ uint32_t *stats_index)
{
    SWSS_LOG_ENTER();

    std::string serializedObjectId = route_obj->get_id();
    sai_route_entry_t route_entry;
    sai_deserialize_route_entry(serializedObjectId, route_entry);

    // Get VRF
    std::shared_ptr<IpVrfInfo> vrf;
    uint32_t vrf_id;

    vrf = vpp_get_ip_vrf(route_entry.vr_id);
    if (vrf == nullptr) {
        vrf_id = 0;
    } else {
        vrf_id = vrf->m_vrf_id;
    }

    // Allocate route with single nexthop
    vpp_ip_route_t *ip_route = (vpp_ip_route_t *)
        calloc(1, sizeof(vpp_ip_route_t) + sizeof(vpp_ip_nexthop_t));
    if (!ip_route) {
        SWSS_LOG_ERROR("Failed to allocate memory for ip_route");
        return SAI_STATUS_FAILURE;
    }

    create_route_prefix_entry(&route_entry, ip_route);
    ip_route->vrf_id = vrf_id;
    ip_route->is_multipath = true;  // Tell VPP to add/remove a path, not replace the route
    ip_route->nexthop_cnt = 1;

    const char *member_hwif = NULL;
    std::string member_hwif_str;

    if (member->rif_oid != SAI_NULL_OBJECT_ID)
    {
        sai_attribute_t rif_attr;
        uint16_t vlan_id = 0;

        rif_attr.id = SAI_ROUTER_INTERFACE_ATTR_TYPE;
        if (get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, member->rif_oid, 1, &rif_attr) == SAI_STATUS_SUCCESS &&
            rif_attr.value.s32 == SAI_ROUTER_INTERFACE_TYPE_SUB_PORT)
        {
            sai_attribute_t vlan_attr;
            vlan_attr.id = SAI_ROUTER_INTERFACE_ATTR_OUTER_VLAN_ID;

            if (get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, member->rif_oid, 1, &vlan_attr) == SAI_STATUS_SUCCESS)
            {
                vlan_id = vlan_attr.value.u16;
            }
        }

        rif_attr.id = SAI_ROUTER_INTERFACE_ATTR_PORT_ID;
        if (get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, member->rif_oid, 1, &rif_attr) == SAI_STATUS_SUCCESS &&
            vpp_get_hwif_name(rif_attr.value.oid, vlan_id, member_hwif_str))
        {
            member_hwif = member_hwif_str.c_str();
        }
    }

    create_vpp_nexthop_entry(member, member_hwif, VPP_NEXTHOP_NORMAL, &ip_route->nexthop[0]);

    int ret = ip_route_add_del_get_stats(ip_route, is_add, stats_index);

    SWSS_LOG_NOTICE("%s path in route %s status %d table %u",
                    (is_add ? "Add" : "Remove"), serializedObjectId.c_str(), ret, vrf_id);

    free(ip_route);

    return (ret == 0) ? SAI_STATUS_SUCCESS : SAI_STATUS_FAILURE;
}

sai_status_t SwitchVpp::addIpRoute(
        _In_ const std::string &serializedObjectId,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    bool isLoopback = false;
    bool isTunnelNh = false;
    bool isSRv6Nh = false;
    bool hasNexthopAttr = false;
    bool route_programmed = false;
    uint32_t stats_index = UINT32_MAX;
    sai_object_id_t counter_oid = SAI_NULL_OBJECT_ID;
    sai_status_t status = SAI_STATUS_SUCCESS;

    SaiCachedObject ip_route_obj(this, SAI_OBJECT_TYPE_ROUTE_ENTRY, serializedObjectId, attr_count, attr_list);

    sai_attribute_t counter_attr;
    counter_attr.id = SAI_ROUTE_ENTRY_ATTR_COUNTER_ID;
    if (ip_route_obj.get_attr(counter_attr) == SAI_STATUS_SUCCESS)
    {
        counter_oid = counter_attr.value.oid;
        std::string boundRoute;
        if (counter_oid != SAI_NULL_OBJECT_ID &&
            getCounterBoundRoute(counter_oid, boundRoute) &&
            boundRoute != serializedObjectId)
        {
            SWSS_LOG_ERROR("counter %s is already bound to route %s",
                    sai_serialize_object_id(counter_oid).c_str(),
                    boundRoute.c_str());
            return SAI_STATUS_OBJECT_IN_USE;
        }
    }

    /* Check if route has a NEXT_HOP_ID pointing to a real nexthop (NH or NHG).
     * Routes pointing to PORT or ROUTER_INTERFACE (e.g. Loopback0 IP2ME routes)
     * still need the loopback check. */
    {
        sai_attribute_t nh_attr;
        nh_attr.id = SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID;
        if (ip_route_obj.get_attr(nh_attr) == SAI_STATUS_SUCCESS &&
            nh_attr.value.oid != SAI_NULL_OBJECT_ID)
        {
            auto oid_type = RealObjectIdManager::objectTypeQuery(nh_attr.value.oid);
            if (oid_type == SAI_OBJECT_TYPE_NEXT_HOP ||
                oid_type == SAI_OBJECT_TYPE_NEXT_HOP_GROUP)
            {
                hasNexthopAttr = true;
            }
        }
    }

    // Only check nexthop type if it's actually a NEXT_HOP (not NEXT_HOP_GROUP)
    sai_attribute_t nh_id_attr;
    nh_id_attr.id = SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID;
    if (ip_route_obj.get_attr(nh_id_attr) == SAI_STATUS_SUCCESS &&
        SAI_OBJECT_TYPE_NEXT_HOP == RealObjectIdManager::objectTypeQuery(nh_id_attr.value.oid))
    {
        auto nh_obj = ip_route_obj.get_linked_object(SAI_OBJECT_TYPE_NEXT_HOP, SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID);
        if (nh_obj != nullptr) {
            sai_attribute_t attr;
            attr.id = SAI_NEXT_HOP_ATTR_TYPE;
            CHECK_STATUS_W_MSG(nh_obj->get_attr(attr), "Missing SAI_NEXT_HOP_ATTR_TYPE in tunnel obj");
            isTunnelNh = (attr.value.s32 == SAI_NEXT_HOP_TYPE_TUNNEL_ENCAP);
            isSRv6Nh = (attr.value.s32 == SAI_NEXT_HOP_TYPE_SRV6_SIDLIST);
        }
    }

    auto removeProgrammedRoute = [&]() -> sai_status_t
    {
        if (!route_programmed)
        {
            return SAI_STATUS_SUCCESS;
        }

        if (isSRv6Nh)
        {
            return m_tunnel_mgr_srv6.remove_sidlist_route_entry(serializedObjectId, nh_id_attr.value.oid);
        }

        return IpRouteAddRemove(&ip_route_obj, false, NULL);
    };

    if (isTunnelNh) {
        status = IpRouteAddRemove(&ip_route_obj, true, &stats_index);
        route_programmed = (status == SAI_STATUS_SUCCESS);
    } else {
        /*
         * Loopback check: only when there is no nexthop attribute.
         * Routes with a valid NEXT_HOP_ID (NH or NHG) are never loopback.
         * process_interface_loopback forks 4 shell processes per call (~4ms).
         */
        if (!hasNexthopAttr) {
            process_interface_loopback(serializedObjectId, isLoopback, true);
        }
        if (isLoopback == false && is_ip_nbr_active() == true)
        {
            if (isSRv6Nh) {
                status = m_tunnel_mgr_srv6.create_sidlist_route_entry(serializedObjectId, switch_id, attr_count, attr_list);
            } else {
                status = IpRouteAddRemove(&ip_route_obj, true, &stats_index);
            }
            route_programmed = (status == SAI_STATUS_SUCCESS);
        }
    }

    if (status != SAI_STATUS_SUCCESS)
    {
        return status;
    }

    if (counter_oid != SAI_NULL_OBJECT_ID && stats_index == UINT32_MAX)
    {
        SWSS_LOG_ERROR("route %s cannot bind counter %s without a VPP stats index",
                serializedObjectId.c_str(),
                sai_serialize_object_id(counter_oid).c_str());
        if (removeProgrammedRoute() != SAI_STATUS_SUCCESS)
        {
            return SAI_STATUS_FAILURE;
        }
        return SAI_STATUS_NOT_SUPPORTED;
    }

    if (stats_index != UINT32_MAX)
    {
        m_routeStatsIndexMap[serializedObjectId] = stats_index;
    }

    status = setRouteCounterBinding(serializedObjectId, counter_oid);
    if (status != SAI_STATUS_SUCCESS)
    {
        m_routeStatsIndexMap.erase(serializedObjectId);
        if (removeProgrammedRoute() != SAI_STATUS_SUCCESS)
        {
            return SAI_STATUS_FAILURE;
        }
        return status;
    }

    status = create_internal(SAI_OBJECT_TYPE_ROUTE_ENTRY, serializedObjectId, switch_id, attr_count, attr_list);
    if (status != SAI_STATUS_SUCCESS)
    {
        // The route (and its COUNTER_ID) was not committed, so the relationship
        // does not exist; erase the base/carry snapshot by counter OID directly.
        if (counter_oid != SAI_NULL_OBJECT_ID)
        {
            m_routeCounterStatsBaseMap.erase(counter_oid);
            m_routeCounterStatsCarryMap.erase(counter_oid);
        }
        m_routeStatsIndexMap.erase(serializedObjectId);
        if (removeProgrammedRoute() != SAI_STATUS_SUCCESS)
        {
            return SAI_STATUS_FAILURE;
        }
        return status;
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::updateIpRoute(
        _In_ const std::string &serializedObjectId,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    auto rollbackRoute = [&](
            const SaiObject* old_route_obj,
            const SaiObject* new_route_obj,
            uint32_t old_stats_index) -> sai_status_t
    {
        if (new_route_obj)
        {
            auto delete_status = IpRouteAddRemove(new_route_obj, false, NULL);
            if (delete_status != SAI_STATUS_SUCCESS)
            {
                SWSS_LOG_ERROR("failed to delete new route %s during rollback, status %d",
                        serializedObjectId.c_str(),
                        delete_status);
                return SAI_STATUS_FAILURE;
            }
        }

        uint32_t rollback_stats_index = UINT32_MAX;
        auto rollback_status = IpRouteAddRemove(old_route_obj, true, &rollback_stats_index);
        if (rollback_status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("failed to rollback route %s, status %d",
                    serializedObjectId.c_str(),
                    rollback_status);
            return SAI_STATUS_FAILURE;
        }

        if (rollback_stats_index != UINT32_MAX)
        {
            m_routeStatsIndexMap[serializedObjectId] = rollback_stats_index;
        }
        else if (old_stats_index != UINT32_MAX)
        {
            m_routeStatsIndexMap[serializedObjectId] = old_stats_index;
            SWSS_LOG_WARN("rollback route %s restored old cached stats index %u because VPP did not return a new index",
                    serializedObjectId.c_str(),
                    old_stats_index);
        }

        return SAI_STATUS_SUCCESS;
    };

    if (attr->id == SAI_ROUTE_ENTRY_ATTR_COUNTER_ID)
    {
        // Read the committed old binding before set_internal persists the new one.
        sai_object_id_t old_counter_oid = getRouteBoundCounter(serializedObjectId);

        if (attr->value.oid != SAI_NULL_OBJECT_ID &&
            m_routeStatsIndexMap.find(serializedObjectId) == m_routeStatsIndexMap.end())
        {
            SWSS_LOG_ERROR("route %s cannot bind counter %s without a VPP stats index",
                    serializedObjectId.c_str(),
                    sai_serialize_object_id(attr->value.oid).c_str());
            return SAI_STATUS_NOT_SUPPORTED;
        }

        sai_status_t status = setRouteCounterBinding(serializedObjectId, attr->value.oid);
        if (status != SAI_STATUS_SUCCESS)
        {
            return status;
        }

        status = set_internal(SAI_OBJECT_TYPE_ROUTE_ENTRY, serializedObjectId, attr);
        if (status != SAI_STATUS_SUCCESS)
        {
            if (setRouteCounterBinding(serializedObjectId, old_counter_oid) != SAI_STATUS_SUCCESS)
            {
                return SAI_STATUS_FAILURE;
            }
        }

        return status;
    }

    if (is_ip_nbr_active() == true) {
        SWSS_LOG_NOTICE("ip route entry update %s", serializedObjectId.c_str());
        SaiModDBObject route_mod_obj(this, SAI_OBJECT_TYPE_ROUTE_ENTRY, serializedObjectId, 1, attr);
        uint32_t stats_index = UINT32_MAX;
        uint32_t old_stats_index = UINT32_MAX;
        auto oldStatsIt = m_routeStatsIndexMap.find(serializedObjectId);
        if (oldStatsIt != m_routeStatsIndexMap.end())
        {
            old_stats_index = oldStatsIt->second;
        }

        auto route_db_obj = route_mod_obj.get_db_obj();
        if (!route_db_obj) {
            SWSS_LOG_ERROR("Failed to find SAI_OBJECT_TYPE_ROUTE_ENTRY SaiObject: %s", serializedObjectId.c_str());
            return SAI_STATUS_FAILURE;
        }

        sai_object_id_t counter_oid = getRouteBoundCounter(serializedObjectId);
        std::map<sai_stat_id_t, uint64_t> old_counter_stats;
        bool has_old_counter_stats = false;
        if (counter_oid != SAI_NULL_OBJECT_ID)
        {
            auto status = getRouteCounterStats(counter_oid, old_counter_stats);
            if (status != SAI_STATUS_SUCCESS)
            {
                return status;
            }
            has_old_counter_stats = true;
        }

        {
            auto status = IpRouteAddRemove(route_db_obj.get(), false, NULL);
            if (status != SAI_STATUS_SUCCESS)
            {
                return status;
            }
            m_routeStatsIndexMap.erase(serializedObjectId);
        }

        auto status = IpRouteAddRemove(&route_mod_obj, true, &stats_index);
        if (status != SAI_STATUS_SUCCESS)
        {
            auto rollback_status = rollbackRoute(route_db_obj.get(), NULL, old_stats_index);
            if (rollback_status != SAI_STATUS_SUCCESS)
            {
                return rollback_status;
            }
            return status;
        }

        if (stats_index != UINT32_MAX)
        {
            m_routeStatsIndexMap[serializedObjectId] = stats_index;
        }
        else if (counter_oid != SAI_NULL_OBJECT_ID)
        {
            auto rollback_status = rollbackRoute(route_db_obj.get(), &route_mod_obj, old_stats_index);
            if (rollback_status != SAI_STATUS_SUCCESS)
            {
                return rollback_status;
            }
            SWSS_LOG_WARN("route %s cannot keep counter binding because VPP did not return a stats index",
                    serializedObjectId.c_str());
            return SAI_STATUS_NOT_SUPPORTED;
        }

        std::map<sai_stat_id_t, uint64_t> new_counter_base;
        bool has_new_counter_base = false;
        if (counter_oid != SAI_NULL_OBJECT_ID)
        {
            status = getRouteCounterStats(counter_oid, new_counter_base);
            if (status != SAI_STATUS_SUCCESS)
            {
                auto rollback_status = rollbackRoute(route_db_obj.get(), &route_mod_obj, old_stats_index);
                if (rollback_status != SAI_STATUS_SUCCESS)
                {
                    return rollback_status;
                }
                return status;
            }
            has_new_counter_base = true;
        }

        status = set_internal(SAI_OBJECT_TYPE_ROUTE_ENTRY, serializedObjectId, attr);
        if (status != SAI_STATUS_SUCCESS)
        {
            auto rollback_status = rollbackRoute(route_db_obj.get(), &route_mod_obj, old_stats_index);
            if (rollback_status != SAI_STATUS_SUCCESS)
            {
                return rollback_status;
            }
            return status;
        }

        if (has_new_counter_base)
        {
            if (has_old_counter_stats)
            {
                carryRouteCounterStatsDelta(counter_oid, old_counter_stats);
            }
            m_routeCounterStatsBaseMap[counter_oid] = new_counter_base;
        }

        return SAI_STATUS_SUCCESS;
    }

    sai_object_id_t counter_oid = getRouteBoundCounter(serializedObjectId);
    std::map<sai_stat_id_t, uint64_t> old_counter_stats;
    bool has_old_counter_stats = false;
    if (counter_oid != SAI_NULL_OBJECT_ID)
    {
        CHECK_STATUS(getRouteCounterStats(counter_oid, old_counter_stats));
        has_old_counter_stats = true;
    }

    std::map<sai_stat_id_t, uint64_t> new_counter_base;
    bool has_new_counter_base = false;
    if (counter_oid != SAI_NULL_OBJECT_ID)
    {
        CHECK_STATUS(getRouteCounterStats(counter_oid, new_counter_base));
        has_new_counter_base = true;
    }

    CHECK_STATUS(set_internal(SAI_OBJECT_TYPE_ROUTE_ENTRY, serializedObjectId, attr));

    if (has_new_counter_base)
    {
        if (has_old_counter_stats)
        {
            carryRouteCounterStatsDelta(counter_oid, old_counter_stats);
        }
        m_routeCounterStatsBaseMap[counter_oid] = new_counter_base;
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::removeIpRoute(
        _In_ const std::string &serializedObjectId)
{
    SWSS_LOG_ENTER();

    bool isLoopback = false;
    bool isSRv6Nh = false;
    bool hasNexthopAttr = false;
    sai_attribute_t attr;
    sai_object_id_t nh_oid;

    /*
     * Check if the route has a nexthop before the loopback check.
     * Routes with a valid NEXT_HOP_ID (NH or NHG) are never loopback.
     * process_interface_loopback forks 4 shell processes per call (~4ms).
     */
    auto route_obj = get_sai_object(SAI_OBJECT_TYPE_ROUTE_ENTRY, serializedObjectId);
    if (route_obj) {
        attr.id = SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID;
        if (route_obj->get_attr(attr) == SAI_STATUS_SUCCESS &&
            attr.value.oid != SAI_NULL_OBJECT_ID)
        {
            auto oid_type = RealObjectIdManager::objectTypeQuery(attr.value.oid);
            if (oid_type == SAI_OBJECT_TYPE_NEXT_HOP ||
                oid_type == SAI_OBJECT_TYPE_NEXT_HOP_GROUP)
            {
                hasNexthopAttr = true;
            }
        }
    }

    if (!hasNexthopAttr) {
        process_interface_loopback(serializedObjectId, isLoopback, false);
    }

    if (isLoopback == false && is_ip_nbr_active() == true) {
	    if (route_obj) {
            attr.id = SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID;
            if(route_obj->get_attr(attr) == SAI_STATUS_SUCCESS) {
                nh_oid = attr.value.oid;
                // Only check nexthop type if it's actually a NEXT_HOP (not NEXT_HOP_GROUP)
                if (SAI_OBJECT_TYPE_NEXT_HOP == RealObjectIdManager::objectTypeQuery(nh_oid)) {
                    attr.id = SAI_NEXT_HOP_ATTR_TYPE;
                    if(get(SAI_OBJECT_TYPE_NEXT_HOP, nh_oid, 1, &attr) == SAI_STATUS_SUCCESS) {
                        isSRv6Nh = (attr.value.s32 == SAI_NEXT_HOP_TYPE_SRV6_SIDLIST);
                    }
                }
            }

            if (isSRv6Nh) {
                m_tunnel_mgr_srv6.remove_sidlist_route_entry(serializedObjectId, nh_oid);
            } else {
                CHECK_STATUS(IpRouteAddRemove(route_obj.get(), false));
            }
	    }
    }

    // Capture the bound counter before remove_internal tears down the route's
    // SaiObjectDB relationship, so its base/carry tables can still be cleaned up.
    // route_obj is the committed object fetched above; reuse it to avoid a
    // redundant SaiObjectDB lookup.
    sai_object_id_t counter_oid = getRouteBoundCounter(route_obj.get());

    CHECK_STATUS(remove_internal(SAI_OBJECT_TYPE_ROUTE_ENTRY, serializedObjectId));

    if (counter_oid != SAI_NULL_OBJECT_ID)
    {
        m_routeCounterStatsBaseMap.erase(counter_oid);
        m_routeCounterStatsCarryMap.erase(counter_oid);
    }
    m_routeStatsIndexMap.erase(serializedObjectId);

    return SAI_STATUS_SUCCESS;
}
