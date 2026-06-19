#include "SwitchVpp.h"

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
#include "SwitchVppNexthop.h"

using namespace saivs;

void create_route_prefix_entry(sai_route_entry_t *route_entry, vpp_ip_route_t *ip_route);
void create_vpp_nexthop_entry(nexthop_grp_member_t *nxt_grp_member, const char *hwif_name,
        vpp_nexthop_type_e type, vpp_ip_nexthop_t *vpp_nexthop);

#define CHECK_STATUS_QUIET(status) {                        \
    sai_status_t _status = (status);                        \
    if (_status != SAI_STATUS_SUCCESS) { return _status; } }

static sai_ip_prefix_t make_neighbor_host_prefix(const sai_ip_address_t &addr)
{
    sai_ip_prefix_t prefix;

    prefix.addr_family = addr.addr_family;
    prefix.addr = addr.addr;

    if (addr.addr_family == SAI_IP_ADDR_FAMILY_IPV4)
    {
        memset(&prefix.mask.ip4, 0xFF, sizeof(prefix.mask.ip4));
    }
    else
    {
        memset(prefix.mask.ip6, 0xFF, sizeof(prefix.mask.ip6));
    }

    return prefix;
}

static bool neighbor_no_host_route_from_attrs(
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    for (uint32_t i = 0; i < attr_count; i++)
    {
        if (attr_list[i].id == SAI_NEIGHBOR_ENTRY_ATTR_NO_HOST_ROUTE)
        {
            return attr_list[i].value.booldata;
        }
    }

    return false;
}

sai_status_t SwitchVpp::programNeighborHostRoute(
        _In_ const std::string &serializedObjectId,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list,
        _In_ bool is_add)
{
    SWSS_LOG_ENTER();

    bool no_host_route = false;

    if (is_add)
    {
        no_host_route = neighbor_no_host_route_from_attrs(attr_count, attr_list);
    }
    else
    {
        sai_attribute_t attr;
        attr.id = SAI_NEIGHBOR_ENTRY_ATTR_NO_HOST_ROUTE;

        if (get(SAI_OBJECT_TYPE_NEIGHBOR_ENTRY, serializedObjectId, 1, &attr) == SAI_STATUS_SUCCESS)
        {
            no_host_route = attr.value.booldata;
        }
    }

    if (no_host_route)
    {
        return SAI_STATUS_SUCCESS;
    }

    sai_neighbor_entry_t nbr_entry;
    sai_deserialize_neighbor_entry(serializedObjectId, nbr_entry);

    sai_attribute_t attr;
    sai_object_id_t vr_id;

    attr.id = SAI_ROUTER_INTERFACE_ATTR_TYPE;
    CHECK_STATUS_QUIET(get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, nbr_entry.rif_id, 1, &attr));

    if (attr.value.s32 != SAI_ROUTER_INTERFACE_TYPE_SUB_PORT &&
        attr.value.s32 != SAI_ROUTER_INTERFACE_TYPE_PORT)
    {
        SWSS_LOG_NOTICE("Skipping neighbor host route for RIF type %d", attr.value.s32);
        return SAI_STATUS_SUCCESS;
    }

    attr.id = SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID;
    CHECK_STATUS_QUIET(get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, nbr_entry.rif_id, 1, &attr));
    vr_id = attr.value.oid;

    std::shared_ptr<IpVrfInfo> vrf = vpp_get_ip_vrf(vr_id);
    uint32_t vrf_id = (vrf == nullptr) ? 0 : vrf->m_vrf_id;

    attr.id = SAI_ROUTER_INTERFACE_ATTR_PORT_ID;
    CHECK_STATUS_QUIET(get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, nbr_entry.rif_id, 1, &attr));

    uint16_t vlan_id = 0;
    sai_attribute_t rif_type_attr;
    rif_type_attr.id = SAI_ROUTER_INTERFACE_ATTR_TYPE;

    if (get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, nbr_entry.rif_id, 1, &rif_type_attr) == SAI_STATUS_SUCCESS &&
        rif_type_attr.value.s32 == SAI_ROUTER_INTERFACE_TYPE_SUB_PORT)
    {
        sai_attribute_t vlan_attr;
        vlan_attr.id = SAI_ROUTER_INTERFACE_ATTR_OUTER_VLAN_ID;

        if (get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, nbr_entry.rif_id, 1, &vlan_attr) == SAI_STATUS_SUCCESS)
        {
            vlan_id = vlan_attr.value.u16;
        }
    }

    std::string hwif_name;
    if (vpp_get_hwif_name(attr.value.oid, vlan_id, hwif_name) == false)
    {
        SWSS_LOG_ERROR("hw interface for neighbor host route %s not found", serializedObjectId.c_str());
        return SAI_STATUS_FAILURE;
    }

    sai_route_entry_t route_entry;
    route_entry.switch_id = nbr_entry.switch_id;
    route_entry.vr_id = vr_id;
    route_entry.destination = make_neighbor_host_prefix(nbr_entry.ip_address);

    vpp_ip_route_t *ip_route = (vpp_ip_route_t *)
        calloc(1, sizeof(vpp_ip_route_t) + sizeof(vpp_ip_nexthop_t));

    if (!ip_route)
    {
        return SAI_STATUS_FAILURE;
    }

    create_route_prefix_entry(&route_entry, ip_route);
    ip_route->vrf_id = vrf_id;
    ip_route->is_multipath = false;
    ip_route->nexthop_cnt = 1;

    nexthop_grp_member_t member;
    memset(&member, 0, sizeof(member));
    member.addr = nbr_entry.ip_address;
    member.rif_oid = nbr_entry.rif_id;
    member.weight = 1;
    // Force ip_route_add_del() to resolve the egress interface from hwif_name.
    // sw_if_index 0 is VPP's local0: leaving it 0 (from memset) makes the host
    // route's path point at local0 and the packet is dropped, even though the
    // neighbor adjacency over the real interface exists. ~0 selects the
    // hwif_name lookup branch (same convention as fillNHGrpMember()).
    member.sw_if_index = (uint32_t) ~0;

    create_vpp_nexthop_entry(&member, hwif_name.c_str(), VPP_NEXTHOP_NORMAL, &ip_route->nexthop[0]);

    init_vpp_client();
    int ret = ip_route_add_del(ip_route, is_add);

    SWSS_LOG_NOTICE("%s neighbor host route %s status %d vrf %u hwif %s",
                    (is_add ? "Add" : "Remove"), serializedObjectId.c_str(), ret, vrf_id, hwif_name.c_str());

    free(ip_route);

    return (ret == 0) ? SAI_STATUS_SUCCESS : SAI_STATUS_FAILURE;
}

sai_status_t SwitchVpp::addRemoveIpNbr(
        _In_ const std::string &serializedObjectId,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list,
        _In_ bool is_add)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;
    sai_neighbor_entry_t nbr_entry;

    sai_deserialize_neighbor_entry(serializedObjectId, nbr_entry);

    attr.id = SAI_ROUTER_INTERFACE_ATTR_TYPE;
    CHECK_STATUS(get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, nbr_entry.rif_id, 1, &attr));

    int32_t rif_type = attr.value.s32;

    if (rif_type != SAI_ROUTER_INTERFACE_TYPE_SUB_PORT &&
        rif_type != SAI_ROUTER_INTERFACE_TYPE_PORT)
    {
        SWSS_LOG_NOTICE("Skipping neighbor VPP programming for RIF type %d", rif_type);
        return SAI_STATUS_SUCCESS;
    }

    attr.id = SAI_ROUTER_INTERFACE_ATTR_PORT_ID;

    CHECK_STATUS(get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, nbr_entry.rif_id, 1, &attr));

    auto port_obj_type = objectTypeQuery(attr.value.oid);
    if (port_obj_type != SAI_OBJECT_TYPE_PORT && port_obj_type != SAI_OBJECT_TYPE_LAG)
    {
        return SAI_STATUS_SUCCESS;
    }
    auto port_oid = attr.value.oid;

    uint16_t vlan_id = 0;
    if (rif_type == SAI_ROUTER_INTERFACE_TYPE_SUB_PORT)
    {
        attr.id = SAI_ROUTER_INTERFACE_ATTR_OUTER_VLAN_ID;

        CHECK_STATUS(get(SAI_OBJECT_TYPE_ROUTER_INTERFACE, nbr_entry.rif_id, 1, &attr));
        vlan_id = attr.value.u16;
    }

    sai_mac_t nbr_mac;
    bool no_mac = true;

    if (is_add)
    {
        for (uint32_t i = 0; i < attr_count; i++)
        {
            switch (attr_list[i].id)
            {
            case SAI_NEIGHBOR_ENTRY_ATTR_DST_MAC_ADDRESS:
                memcpy(nbr_mac, attr_list[i].value.mac, sizeof(sai_mac_t));
                no_mac = false;
                break;

            default:
                break;
            }
        }
    } else {
        attr.id = SAI_NEIGHBOR_ENTRY_ATTR_DST_MAC_ADDRESS;

        if (get(SAI_OBJECT_TYPE_NEIGHBOR_ENTRY, serializedObjectId, 1, &attr) == SAI_STATUS_SUCCESS) {
            memcpy(nbr_mac, attr.value.mac, sizeof(sai_mac_t));
            no_mac = false;
        }
    }

    if (no_mac == true)
    {
        SWSS_LOG_ERROR("No mac address passed for neighbor %s", serializedObjectId.c_str());
        return SAI_STATUS_FAILURE;
    }

    std::string hwif_name;
    bool found = vpp_get_hwif_name(port_oid, vlan_id, hwif_name);
    if (found == false)
    {
        SWSS_LOG_ERROR("hw interface for port/lag id %s not found", serializedObjectId.c_str());
        return SAI_STATUS_FAILURE;
    }

    const char *vpp_ifname = hwif_name.c_str();
    init_vpp_client();

    switch (nbr_entry.ip_address.addr_family) {
    case SAI_IP_ADDR_FAMILY_IPV4:
        struct sockaddr_in sin;

        sin.sin_family = AF_INET;
        sin.sin_addr.s_addr = nbr_entry.ip_address.addr.ip4;

        ip4_nbr_add_del(vpp_ifname, ~0, &sin, false, false, nbr_mac, is_add);

        break;

    case SAI_IP_ADDR_FAMILY_IPV6:
        struct sockaddr_in6 sin6;

        sin6.sin6_family = AF_INET6;
        memcpy(sin6.sin6_addr.s6_addr, nbr_entry.ip_address.addr.ip6, sizeof(sin6.sin6_addr.s6_addr));

        ip6_nbr_add_del(vpp_ifname, ~0, &sin6, false, false, nbr_mac, is_add);

        break;
    }

    return SAI_STATUS_SUCCESS;
}

bool SwitchVpp::is_ip_nbr_active()
{
    SWSS_LOG_ENTER();

    if (nbr_env_read == false)
    {
        const char *val;

        val = getenv("NO_LINUX_NL");
        if (val && (*val == 'n' || *val == 'N')) {
            nbr_active = false;
        }
        nbr_env_read = true;
    }
    return nbr_active;
}

sai_status_t SwitchVpp::addIpNbr(
        _In_ const std::string &serializedObjectId,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    if (is_ip_nbr_active() == true) {
        SWSS_LOG_NOTICE("Add neighbor in VS %s", serializedObjectId.c_str());
        CHECK_STATUS(addRemoveIpNbr(serializedObjectId, attr_count, attr_list, true));
    }

    CHECK_STATUS(create_internal(SAI_OBJECT_TYPE_NEIGHBOR_ENTRY, serializedObjectId, switch_id, attr_count, attr_list));

    if (is_ip_nbr_active() == true) {
        CHECK_STATUS(programNeighborHostRoute(serializedObjectId, attr_count, attr_list, true));
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::removeIpNbr(
        _In_ const std::string &serializedObjectId)
{
    SWSS_LOG_ENTER();

    if (is_ip_nbr_active() == true) {
        SWSS_LOG_NOTICE("Remove neighbor in VS %s", serializedObjectId.c_str());
        CHECK_STATUS(programNeighborHostRoute(serializedObjectId, 0, NULL, false));
        CHECK_STATUS(addRemoveIpNbr(serializedObjectId, 0, NULL, false));
    }

    CHECK_STATUS(remove_internal(SAI_OBJECT_TYPE_NEIGHBOR_ENTRY, serializedObjectId));

    return SAI_STATUS_SUCCESS;
}
