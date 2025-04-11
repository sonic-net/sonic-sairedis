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

static void create_route_prefix_entry (
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
            _In_ bool is_add)
{
    SWSS_LOG_ENTER();

    int ret = SAI_STATUS_SUCCESS;
    sai_status_t                 status;
    sai_object_id_t              next_hop_oid;
    sai_attribute_t              attr;
    std::string                  serializedObjectId = route_obj->get_id();

    attr.id = SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID;
    CHECK_STATUS_QUIET(route_obj->get_mandatory_attr(attr));
    next_hop_oid = attr.value.oid;

    sai_route_entry_t route_entry;
    const char *hwif_name = NULL;
    vpp_nexthop_type_e nexthop_type = VPP_NEXTHOP_NORMAL;
    bool config_ip_route = false;

    sai_deserialize_route_entry(serializedObjectId, route_entry);

    nexthop_grp_config_t *nxthop_group = NULL;

    if (SAI_OBJECT_TYPE_ROUTER_INTERFACE == RealObjectIdManager::objectTypeQuery(next_hop_oid))
    {
        // vpp_add_del_intf_ip_addr(route_entry.destination, next_hop_oid, is_add);
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

        size_t i;
        for (i = 0; i < nxthop_group->nmembers; i++) {
            create_vpp_nexthop_entry(nxt_grp_member, hwif_name, nexthop_type,  &ip_route->nexthop[i]);
            nxt_grp_member++;
        }
        ip_route->nexthop_cnt = nxthop_group->nmembers;

        ret = ip_route_add_del(ip_route, is_add);

        SWSS_LOG_NOTICE("%s ip route in VS %s status %d table %u", (is_add ? "Add" : "Remove"),
                        serializedObjectId.c_str(), ret, vrf_id);
        SWSS_LOG_NOTICE("%s route nexthop type %s count %u", (is_add ? "Add" : "Remove"),
                        sai_serialize_object_type(RealObjectIdManager::objectTypeQuery(next_hop_oid)).c_str(),
                        nxthop_group->nmembers);

        free(ip_route);
        free(nxthop_group);

    } else {
        SWSS_LOG_NOTICE("Ignoring VS ip route %s", serializedObjectId.c_str());
    }

    return ret;
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

    SaiCachedObject ip_route_obj(this, SAI_OBJECT_TYPE_ROUTE_ENTRY, serializedObjectId, attr_count, attr_list);
    auto nh_obj = ip_route_obj.get_linked_object(SAI_OBJECT_TYPE_NEXT_HOP, SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID);
    if (nh_obj != nullptr) {
        sai_attribute_t attr;
        attr.id = SAI_NEXT_HOP_ATTR_TYPE;
        CHECK_STATUS_W_MSG(nh_obj->get_attr(attr), "Missing SAI_NEXT_HOP_ATTR_TYPE in tunnel obj");
        isTunnelNh = (attr.value.s32 == SAI_NEXT_HOP_TYPE_TUNNEL_ENCAP);
        isSRv6Nh = (attr.value.s32 == SAI_NEXT_HOP_TYPE_SRV6_SIDLIST);
    }

    if (isTunnelNh) {
        IpRouteAddRemove(&ip_route_obj, true);
    } else {
        process_interface_loopback(serializedObjectId, isLoopback, true);
        if (isLoopback == false && is_ip_nbr_active() == true)
        {
            if (isSRv6Nh) {
                m_tunnel_mgr_srv6.create_sidlist_route_entry(serializedObjectId, switch_id, attr_count, attr_list);
            } else {
                IpRouteAddRemove(&ip_route_obj, true);
            }
        }
    }

    CHECK_STATUS(create_internal(SAI_OBJECT_TYPE_ROUTE_ENTRY, serializedObjectId, switch_id, attr_count, attr_list));

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::updateIpRoute(
        _In_ const std::string &serializedObjectId,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    if (is_ip_nbr_active() == true) {
        SWSS_LOG_NOTICE("ip route entry update %s", serializedObjectId.c_str());
        SaiModDBObject route_mod_obj(this, SAI_OBJECT_TYPE_ROUTE_ENTRY, serializedObjectId, 1, attr);

        auto route_db_obj = route_mod_obj.get_db_obj();
        if (!route_db_obj) {
            SWSS_LOG_ERROR("Failed to find SAI_OBJECT_TYPE_ROUTE_ENTRY SaiObject: %s", serializedObjectId.c_str());
            return SAI_STATUS_FAILURE;
        } else {
            IpRouteAddRemove(route_db_obj.get(), false);
        }

            IpRouteAddRemove(&route_mod_obj, true);
    }

    set_internal(SAI_OBJECT_TYPE_ROUTE_ENTRY, serializedObjectId, attr);

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::removeIpRoute(
        _In_ const std::string &serializedObjectId)
{
    SWSS_LOG_ENTER();

    bool isLoopback = false;
    bool isSRv6Nh = false;
    sai_attribute_t attr;
    sai_object_id_t nh_oid;
    process_interface_loopback(serializedObjectId, isLoopback, false);

    if (isLoopback == false && is_ip_nbr_active() == true) {
        auto route_obj = get_sai_object(SAI_OBJECT_TYPE_ROUTE_ENTRY, serializedObjectId);
	    if (route_obj) {
            attr.id = SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID;
            if(route_obj->get_attr(attr) == SAI_STATUS_SUCCESS) {
                nh_oid = attr.value.oid;
                attr.id = SAI_NEXT_HOP_ATTR_TYPE;
                if(get(SAI_OBJECT_TYPE_NEXT_HOP, nh_oid, 1, &attr) == SAI_STATUS_SUCCESS) {
                    isSRv6Nh = (attr.value.s32 == SAI_NEXT_HOP_TYPE_SRV6_SIDLIST);
                }
            }

            if (isSRv6Nh) {
                m_tunnel_mgr_srv6.remove_sidlist_route_entry(serializedObjectId, nh_oid);
            } else {
	            IpRouteAddRemove(route_obj.get(), false);
            }
	    }
    }

    CHECK_STATUS(remove_internal(SAI_OBJECT_TYPE_ROUTE_ENTRY, serializedObjectId));

    return SAI_STATUS_SUCCESS;
}
