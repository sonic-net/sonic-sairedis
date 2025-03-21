#include "SwitchVppUtils.h"
#include "SwitchVpp.h"
#include "SaiObjectDB.h"
#include "TunnelManager.h"
#include "IpVrfInfo.h"

#include "meta/sai_serialize.h"

#include "swss/logger.h"

#include "vppxlate/SaiVppXlate.h"

using namespace saivs;

#define CHECK_STATUS_W_MSG(status, msg, ...) {                                  \
    sai_status_t _status = (status);                            \
    if (_status != SAI_STATUS_SUCCESS) { \
        char buffer[512]; \
        snprintf(buffer, 512, msg, ##__VA_ARGS__); \
        SWSS_LOG_ERROR("%s: status %d", buffer, status); \
        return _status; } }

TunnelManager::TunnelManager(SwitchVpp* switch_db): m_switch_db(switch_db)
{
    SWSS_LOG_ENTER();

    m_router_mac = {0, 0, 0, 0, 0, 1};
    m_vxlan_port = 4789;
}

const std::array<uint8_t, 6>&
TunnelManager::get_router_mac() const
{
    SWSS_LOG_ENTER();

    return m_router_mac;
}

void
TunnelManager::set_router_mac(const sai_attribute_t* attr)
{
    SWSS_LOG_ENTER();

    for (int i = 0; i < 6; ++i) {
        m_router_mac[i] = attr->value.mac[i];
    }
}

void
TunnelManager::set_vxlan_port(const sai_attribute_t* attr)
{
    SWSS_LOG_ENTER();

    m_vxlan_port = attr->value.u16;
}
/**
 * VxLAN tunnel is created in response to the creation of a tunnel encap nexthop entry. This assumes VxLAN tunnel is bidirectional and symmetric.
 * The local VTEP sends packet through the tunnel to the remote VTEP. The remote VTEP sends packet back to the local VTEP through the same tunnel with the same VNI.
 * Here is the VS config to be programmed in response to the creation of a tunnel encap nexthop entry:
 *
 * create vxlan tunnel src 1.0.0.1 dst 1.0.0.2 vni 3000
 * ip neighbor vxlan_tunnel0 1.0.0.2 00:00:00:00:00:01 no-fib-entry
 * ip route add 100.1.1.0/24 via 1.0.0.2 vxlan_tunnel0
 *
 * bvi create mac 00:00:00:00:00:01
 * set interface state bvi0 up
 * set interface ip address bvi0 0.0.0.2/32
 * set interface l2 bridge vxlan_tunnel0 3000 1
 * set interface l2 bridge bvi0 3000 bvi
 *
 * corresponding to below sonic config
 *   In CONFIG_DB
 *   "VXLAN_TUNNEL": {
 *        "test": {
 *           "src_ip": "1.0.0.1"
 *       }
 *   }
 *  "VNET": {
 *       "Vnet1": {
 *           "peer_list": "",
 *           "scope": "default",
 *           "vni": "3000",
 *           "vxlan_tunnel": "test"
 *       },
 *   }
 *   In APPL_DB
 *   "VNET_ROUTE_TUNNEL_TABLE:Vnet1:100.1.1.0/24":
 *         {
 *       "endpoint": "1.0.0.2"
 *         }
 */
sai_status_t
TunnelManager::tunnel_encap_nexthop_action(
                    _In_ const SaiObject* tunnel_nh_obj,
                    _In_ Action action)
{
    SWSS_LOG_ENTER();

    sai_attribute_t              attr;
    sai_ip_address_t             src_ip;
    sai_ip_address_t             dst_ip;
    std::unordered_map<u_int32_t, std::shared_ptr<IpVrfInfo>> vni_to_vrf_map;
    sai_object_id_t              object_id;

    SWSS_LOG_DEBUG("tunnel_encap_nexthop_action %s %s",
        action == Action::CREATE ? "CREATE" : "DELETE", tunnel_nh_obj->get_id().c_str());
    sai_deserialize_object_id(tunnel_nh_obj->get_id(), object_id);
    auto tunnel_obj = tunnel_nh_obj->get_linked_object(SAI_OBJECT_TYPE_TUNNEL, SAI_NEXT_HOP_ATTR_TUNNEL_ID);
    if (tunnel_obj == nullptr) {
        return SAI_STATUS_FAILURE;
    }
    attr.id = SAI_TUNNEL_ATTR_TYPE;
    CHECK_STATUS_W_MSG(tunnel_obj->get_attr(attr), "Missing SAI_TUNNEL_ATTR_TYPE in tunnel obj");

    if (attr.value.s32 != SAI_TUNNEL_TYPE_VXLAN) {
        SWSS_LOG_ERROR("Unsupported tunnel encap type %d in %s", attr.value.s32,
                        tunnel_obj->get_id().c_str());
        return SAI_STATUS_NOT_IMPLEMENTED;
    }

    attr.id = SAI_TUNNEL_ATTR_ENCAP_SRC_IP;
    CHECK_STATUS_W_MSG(tunnel_obj->get_attr(attr), "Missing SAI_TUNNEL_ATTR_ENCAP_SRC_IP in tunnel obj");
    // SAI_TUNNEL_ATTR_ENCAP_TTL_MODE and SAI_TUNNEL_ATTR_ENCAP_TTL_VAL are not supported in vpp
    src_ip = attr.value.ipaddr;

    attr.id = SAI_NEXT_HOP_ATTR_IP;
    CHECK_STATUS_W_MSG(tunnel_nh_obj->get_attr(attr), "Missing SAI_NEXT_HOP_ATTR_IP in %s", tunnel_nh_obj->get_id().c_str());

    dst_ip = attr.value.ipaddr;

    // Iterate tunnel encap mapper
    auto tunnel_encap_mappers = tunnel_obj->get_linked_objects(SAI_OBJECT_TYPE_TUNNEL_MAP, SAI_TUNNEL_ATTR_ENCAP_MAPPERS);

    for (auto tunnel_encap_mapper : tunnel_encap_mappers) {
        attr.id = SAI_TUNNEL_MAP_ATTR_TYPE;
        CHECK_STATUS_W_MSG(tunnel_encap_mapper->get_attr(attr),
                "Missing SAI_TUNNEL_MAP_ATTR_TYPE in %s",
                tunnel_encap_mapper->get_id().c_str());
        if (attr.value.s32 != SAI_TUNNEL_MAP_TYPE_VIRTUAL_ROUTER_ID_TO_VNI) {
            continue;
        }

        auto tunnel_encap_mapper_entries = tunnel_encap_mapper->get_child_objs(SAI_OBJECT_TYPE_TUNNEL_MAP_ENTRY);
        if (tunnel_encap_mapper_entries == nullptr) {
            SWSS_LOG_DEBUG("Empty tunnel_encap_mapper table. OID %s",
                tunnel_encap_mapper->get_id().c_str());
            continue;
        }
        for (auto pair : *tunnel_encap_mapper_entries) {
            auto tunnel_encap_mapper_entry = pair.second;
            vpp_vxlan_tunnel_t req;
            u_int32_t tunnel_vni;
            TunnelVPPData tunnel_data;

            memset(&req, 0, sizeof(req));
            req.dst_port = m_vxlan_port;
            req.src_port = m_vxlan_port;
            req.instance = ~0;
            sai_ip_address_t_to_vpp_ip_addr_t(src_ip, req.src_address);
            sai_ip_address_t_to_vpp_ip_addr_t(dst_ip, req.dst_address);
            req.decap_next_index = ~0;

            attr.id = SAI_TUNNEL_MAP_ENTRY_ATTR_VNI_ID_VALUE;
            CHECK_STATUS_W_MSG(tunnel_encap_mapper_entry->get_attr(attr),
                "Missing SAI_TUNNEL_MAP_ENTRY_ATTR_VNI_ID_KEY in %s",
                tunnel_encap_mapper_entry->get_id().c_str());
            tunnel_vni = attr.value.u32;

            attr.id = SAI_TUNNEL_MAP_ENTRY_ATTR_VIRTUAL_ROUTER_ID_KEY;
            CHECK_STATUS_W_MSG(tunnel_encap_mapper_entry->get_attr(attr),
                    "Missing SAI_TUNNEL_MAP_ENTRY_ATTR_VIRTUAL_ROUTER_ID_KEY in %s",
                    tunnel_encap_mapper_entry->get_id().c_str());

            auto ip_vrf = m_switch_db->vpp_get_ip_vrf(attr.value.oid);
            if (!ip_vrf) {
                SWSS_LOG_ERROR("Failed to find VR from SAI_TUNNEL_MAP_ENTRY_ATTR_VIRTUAL_ROUTER_ID_KEY in %s",
                    tunnel_encap_mapper_entry->get_id().c_str());
                return SAI_STATUS_FAILURE;
            }
            vni_to_vrf_map[tunnel_vni] = ip_vrf;
            tunnel_data.ip_vrf = ip_vrf;
            req.vni = tunnel_vni;

            if (action == Action::CREATE) {
                if (create_vpp_vxlan_encap(req, tunnel_data) != SAI_STATUS_SUCCESS) {
                    SWSS_LOG_ERROR("Failed to create vxlan encap for %s",
                        tunnel_nh_obj->get_id().c_str());
                    return SAI_STATUS_FAILURE;
                }

                if (create_vpp_vxlan_decap(tunnel_data) != SAI_STATUS_SUCCESS) {
                    SWSS_LOG_ERROR("Failed to create vxlan decap for %s",
                        tunnel_nh_obj->get_id().c_str());
                    remove_vpp_vxlan_encap(req, tunnel_data);
                    return SAI_STATUS_FAILURE;
                }
                m_tunnel_encap_nexthop_map[object_id] = tunnel_data;

            } else if (action == Action::DELETE) {
                auto encap_map_it = m_tunnel_encap_nexthop_map.find(object_id);
                if (encap_map_it == m_tunnel_encap_nexthop_map.end()) {
                    SWSS_LOG_ERROR("Failed to find sw_if_index for %s",
                        tunnel_nh_obj->get_id().c_str());
                    continue;
                }
                remove_vpp_vxlan_decap(encap_map_it->second);
                remove_vpp_vxlan_encap(req, encap_map_it->second);

                m_tunnel_encap_nexthop_map.erase(encap_map_it);
            }
        }
    }
    return SAI_STATUS_SUCCESS;
}
sai_status_t
TunnelManager::create_tunnel_encap_nexthop(
                    _In_ const std::string& serializedObjectId,
                    _In_ sai_object_id_t switch_id,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    SaiCachedObject tunnel_nh_obj(m_switch_db, SAI_OBJECT_TYPE_NEXT_HOP, serializedObjectId, attr_count, attr_list);
    return tunnel_encap_nexthop_action(&tunnel_nh_obj, Action::CREATE);
}

sai_status_t
TunnelManager::remove_tunnel_encap_nexthop(
                _In_ const std::string& serializedObjectId)
{
    SWSS_LOG_ENTER();

    auto tunnel_nh_obj = m_switch_db->get_sai_object(SAI_OBJECT_TYPE_NEXT_HOP, serializedObjectId);

    if (!tunnel_nh_obj) {
        SWSS_LOG_ERROR("Failed to find SAI_OBJECT_TYPE_NEXT_HOP SaiObject: %s", serializedObjectId.c_str());
        return SAI_STATUS_FAILURE;
    }
    return tunnel_encap_nexthop_action(tunnel_nh_obj.get(), Action::DELETE);
}

sai_status_t
TunnelManager::create_vpp_vxlan_encap(
                    _In_  vpp_vxlan_tunnel_t& req,
                    _Out_ TunnelVPPData& tunnel_data)
{
    SWSS_LOG_ENTER();

    int                         vpp_status;
    u_int32_t                   sw_if_index;
    char                        src_ip_str[INET6_ADDRSTRLEN];
    char                        dst_ip_str[INET6_ADDRSTRLEN];
    auto                        router_mac = get_router_mac();
    auto                        bvi_mac = router_mac.data();

    vpp_status = vpp_vxlan_tunnel_add_del(&req, 1, &sw_if_index);
    vpp_ip_addr_t_to_string(&req.src_address, src_ip_str, INET6_ADDRSTRLEN);
    vpp_ip_addr_t_to_string(&req.dst_address, dst_ip_str, INET6_ADDRSTRLEN);
    SWSS_LOG_INFO("create vxlan tunnel src %s dst %s vni %d: sw_if_index,%d, status %d",
            src_ip_str, dst_ip_str,
            req.vni, sw_if_index, vpp_status);
    if (vpp_status != 0) {
        SWSS_LOG_ERROR("Failed to create vxlan tunnel");
        return SAI_STATUS_FAILURE;
    }
    tunnel_data.sw_if_index = sw_if_index;
    /* the neighbour is to build inner ether. use no_fib_entry to avoid creating the nh in the fib, which will mess up underlay forwarding*/
    if (req.dst_address.sa_family == AF_INET6) {
        ip6_nbr_add_del(NULL, sw_if_index, &req.dst_address.addr.ip6, false, true/*no_fib_entry*/, bvi_mac, 1);
    } else {
        ip4_nbr_add_del(NULL, sw_if_index, &req.dst_address.addr.ip4, false, true/*no_fib_entry*/, bvi_mac, 1);
    }
    SWSS_LOG_INFO("successfully created encap for vxlan tunnel %d", sw_if_index);
    return SAI_STATUS_SUCCESS;
}

sai_status_t
TunnelManager::remove_vpp_vxlan_encap(
                    _In_  vpp_vxlan_tunnel_t& req,
                    _In_ TunnelVPPData& tunnel_data)
{
    SWSS_LOG_ENTER();

    int                         vpp_status;
    u_int32_t                   sw_if_index = tunnel_data.sw_if_index;
    char                        src_ip_str[INET6_ADDRSTRLEN];
    char                        dst_ip_str[INET6_ADDRSTRLEN];
    auto                        router_mac = get_router_mac();
    auto                        bvi_mac = router_mac.data();

    if (req.dst_address.sa_family == AF_INET6) {
        ip6_nbr_add_del(NULL, tunnel_data.sw_if_index, &req.dst_address.addr.ip6, false, true/*no_fib_entry*/, bvi_mac, 0);
    } else {
        ip4_nbr_add_del(NULL, tunnel_data.sw_if_index, &req.dst_address.addr.ip4, false, true/*no_fib_entry*/, bvi_mac, 0);
    }

    vpp_status = vpp_vxlan_tunnel_add_del(&req, 0, &sw_if_index);
    vpp_ip_addr_t_to_string(&req.src_address, src_ip_str, INET6_ADDRSTRLEN);
    vpp_ip_addr_t_to_string(&req.dst_address, dst_ip_str, INET6_ADDRSTRLEN);
    SWSS_LOG_INFO("delete vxlan tunnel src %s dst %s vni %d: sw_if_index %d, status %d",
            src_ip_str, dst_ip_str,
            req.vni, sw_if_index, vpp_status);
    if (vpp_status != 0) {
        SWSS_LOG_ERROR("Failed to delete vxlan tunnel");
        return SAI_STATUS_FAILURE;
    }
    return SAI_STATUS_SUCCESS;
}
sai_status_t
TunnelManager::create_vpp_vxlan_decap(
                    _Out_ TunnelVPPData& tunnel_data)
{
    SWSS_LOG_ENTER();

    int                         vpp_status;
    char                        hw_bvi_ifname[32];
    auto                        router_mac = get_router_mac();
    auto                        bvi_mac = router_mac.data();
    vpp_ip_route_t              bvi_ip_prefix;
    uint32_t                    tunnel_if_index = tunnel_data.sw_if_index;

    //allocate bridge domain ID
    int bd_id = m_switch_db->dynamic_bd_id_pool.alloc();
    if (bd_id == -1) {
        SWSS_LOG_ERROR("Failed to allocate bridge domain ID");
        return SAI_STATUS_FAILURE;
    }
    tunnel_data.bd_id = bd_id;
    //create bvi interface using instance same as bd_id
    vpp_status = create_bvi_interface(bvi_mac, bd_id);
    if (vpp_status != 0) {
        SWSS_LOG_ERROR("Failed to create bvi interface");
        m_switch_db->dynamic_bd_id_pool.free(bd_id);
        return SAI_STATUS_FAILURE;
    }
    // Get new list of physical interfaces from VS
    refresh_interfaces_list();

    //bring up bvi interface
    snprintf(hw_bvi_ifname, sizeof(hw_bvi_ifname), "bvi%u", bd_id);
    vpp_status = interface_set_state(hw_bvi_ifname, true);
    if (vpp_status != 0) {
        SWSS_LOG_ERROR("Failed to bring up bvi interface");
        return SAI_STATUS_FAILURE;
    }

    //Create bridge and set BVI to the BD
    vpp_status = set_sw_interface_l2_bridge(hw_bvi_ifname, bd_id, true, VPP_API_PORT_TYPE_BVI);
    if (vpp_status != 0) {
        SWSS_LOG_ERROR("Failed to add bvi interface to bd");
        return SAI_STATUS_FAILURE;
    }

    //bind bvi to vrf
    vpp_status = set_interface_vrf(hw_bvi_ifname, 0, tunnel_data.ip_vrf->m_vrf_id, tunnel_data.ip_vrf->m_is_ipv6);

    //set bvi IPv4
    uint16_t offset = (uint16_t)((uint16_t)(bd_id - SwitchVpp::dynamic_bd_id_base) + 2);

    bvi_ip_prefix.prefix_len = 32;
    bvi_ip_prefix.prefix_addr.sa_family = AF_INET;
    struct sockaddr_in *sin =  &bvi_ip_prefix.prefix_addr.addr.ip4;
    sin->sin_addr.s_addr = htonl(offset);
    vpp_status = interface_ip_address_add_del(hw_bvi_ifname, &bvi_ip_prefix, true);
    if (vpp_status != 0) {
        SWSS_LOG_ERROR("Failed to config IP on bvi interface");
        return SAI_STATUS_FAILURE;
    }

    //set bvi IPv6 (the same tunnel can carry ipv4 or ipv6)
    bvi_ip_prefix.prefix_len = 128;
    bvi_ip_prefix.prefix_addr.sa_family = AF_INET6;
    struct sockaddr_in6 *sin6 =  &bvi_ip_prefix.prefix_addr.addr.ip6;
    memset(&sin6->sin6_addr, 0, sizeof(struct in6_addr));
    sin6->sin6_addr.s6_addr[14] = (uint8_t)(offset >> 8) & 0xFF;
    sin6->sin6_addr.s6_addr[15] = (uint8_t)(offset & 0xFF);
    vpp_status = interface_ip_address_add_del(hw_bvi_ifname, &bvi_ip_prefix, true);
    if (vpp_status != 0) {
        SWSS_LOG_ERROR("Failed to config IP on bvi interface");
        return SAI_STATUS_FAILURE;
    }

    //set vxlan tunnel to bridge domain
    vpp_status = set_sw_interface_l2_bridge_by_index(tunnel_if_index, bd_id, true, VPP_API_PORT_TYPE_NORMAL);
    if (vpp_status != 0) {
        SWSS_LOG_ERROR("Failed to add tunnel interface to bd");
        return SAI_STATUS_FAILURE;
    }
    SWSS_LOG_INFO("successfully created decap for vxlan tunnel %d with BD %d",
                        tunnel_if_index, bd_id);
    return SAI_STATUS_SUCCESS;
}

sai_status_t
TunnelManager::remove_vpp_vxlan_decap(
                    _In_ TunnelVPPData& tunnel_data)
{
    SWSS_LOG_ENTER();

    char                        hw_bvi_ifname[32];

    snprintf(hw_bvi_ifname, sizeof(hw_bvi_ifname), "bvi%u", tunnel_data.bd_id);

    delete_bvi_interface(hw_bvi_ifname);

    m_switch_db->dynamic_bd_id_pool.free(tunnel_data.bd_id);
    refresh_interfaces_list();
    //bd is create automatically when the fist interface is add to it but requires manual deletion
    vpp_bridge_domain_add_del(tunnel_data.bd_id, false);
    SWSS_LOG_INFO("successfully deleted decap of vxlan tunnel %d with BD %d",
                        tunnel_data.sw_if_index, tunnel_data.bd_id);
    return SAI_STATUS_SUCCESS;
}
