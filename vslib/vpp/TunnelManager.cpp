#include <algorithm>
#include <functional>

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

namespace
{
struct VxlanTermMatch
{
    sai_object_id_t tunnel_oid = SAI_NULL_OBJECT_ID;
    sai_ip_address_t local_ip{};
    sai_ip_address_t remote_ip{};
};

sai_status_t get_exact_vxlan_term_match(
    _In_ const SaiObject* term_obj,
    _Out_ VxlanTermMatch& match,
    _Out_ bool& applicable)
{
    SWSS_LOG_ENTER();

    applicable = false;

    sai_attribute_t attr;

    attr.id = SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_TUNNEL_TYPE;
    if (term_obj->get_attr(attr) != SAI_STATUS_SUCCESS ||
        attr.value.s32 != SAI_TUNNEL_TYPE_VXLAN) {
        return SAI_STATUS_SUCCESS;
    }

    attr.id = SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_TYPE;
    if (term_obj->get_mandatory_attr(attr) != SAI_STATUS_SUCCESS) {
        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }
    if (attr.value.s32 != SAI_TUNNEL_TERM_TABLE_ENTRY_TYPE_P2P) {
        // Source-independent L2 termination is outside the static T0 scope.
        // Preserve the SAI object without silently widening it in VPP.
        return SAI_STATUS_SUCCESS;
    }

    attr.id = SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_ACTION_TUNNEL_ID;
    if (term_obj->get_mandatory_attr(attr) != SAI_STATUS_SUCCESS) {
        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }
    match.tunnel_oid = attr.value.oid;

    attr.id = SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_DST_IP;
    if (term_obj->get_mandatory_attr(attr) != SAI_STATUS_SUCCESS) {
        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }
    match.local_ip = attr.value.ipaddr;

    attr.id = SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_SRC_IP;
    if (term_obj->get_mandatory_attr(attr) != SAI_STATUS_SUCCESS) {
        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }
    match.remote_ip = attr.value.ipaddr;

    if ((match.local_ip.addr_family != SAI_IP_ADDR_FAMILY_IPV4 &&
         match.local_ip.addr_family != SAI_IP_ADDR_FAMILY_IPV6) ||
        match.remote_ip.addr_family != match.local_ip.addr_family) {
        return SAI_STATUS_INVALID_PARAMETER;
    }

    applicable = true;
    return SAI_STATUS_SUCCESS;
}

}

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

sai_status_t
TunnelManager::get_tunnel_if(
    _In_  sai_object_id_t nexthop_oid,
    _In_  sai_object_id_t vrf_oid,
    _Out_ u_int32_t &sw_if_index,
    _In_  bool allow_create)
{
    SWSS_LOG_ENTER();

    auto it = m_tunnel_encap_nexthop_map.find(nexthop_oid);
    if (it != m_tunnel_encap_nexthop_map.end()) {
        auto find_interface = [&]() {
            const auto& entries = it->second;
            if (entries.size() == 1 &&
                (!entries.front().tunnel_data.ip_vrf ||
                 entries.front().tunnel_data.ip_vrf->m_obj_id == vrf_oid)) {
                sw_if_index = entries.front().tunnel_data.sw_if_index;
                return true;
            }
            for (const auto& entry : entries) {
                if (entry.tunnel_data.ip_vrf &&
                    entry.tunnel_data.ip_vrf->m_obj_id == vrf_oid) {
                    sw_if_index = entry.tunnel_data.sw_if_index;
                    return true;
                }
            }
            return false;
        };
        if (find_interface()) {
            return SAI_STATUS_SUCCESS;
        }
        // A later VNET can reuse this next hop in a new VRF. Resolve its
        // current mapper on route creation without replacing active profiles.
        if (allow_create) {
            auto next_hop = m_switch_db->get_sai_object(
                SAI_OBJECT_TYPE_NEXT_HOP, sai_serialize_object_id(nexthop_oid));
            if (!next_hop) {
                return SAI_STATUS_FAILURE;
            }
            sai_status_t status = tunnel_encap_nexthop_action(
                next_hop.get(), Action::UPDATE, vrf_oid);
            if (status != SAI_STATUS_SUCCESS) {
                return status;
            }
            if (find_interface()) {
                return SAI_STATUS_SUCCESS;
            }
        }
        SWSS_LOG_ERROR("No mapped VXLAN VNI for next hop %s in VRF %s",
                       sai_serialize_object_id(nexthop_oid).c_str(),
                       sai_serialize_object_id(vrf_oid).c_str());
        return SAI_STATUS_FAILURE;
    }

    // Fall through to IPIP encap nexthop map
    return m_switch_db->m_tunnel_mgr_ipip.get_tunnel_if(nexthop_oid, sw_if_index);
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
                    _In_ Action action,
                    _In_ sai_object_id_t vrf_oid)
{
    SWSS_LOG_ENTER();

    sai_attribute_t attr;
    sai_object_id_t object_id;

    SWSS_LOG_DEBUG("tunnel_encap_nexthop_action %s %s",
        action == Action::CREATE ? "CREATE" :
        action == Action::UPDATE ? "UPDATE" : "DELETE", tunnel_nh_obj->get_id().c_str());
    sai_deserialize_object_id(tunnel_nh_obj->get_id(), object_id);
    auto tunnel_obj = tunnel_nh_obj->get_linked_object(SAI_OBJECT_TYPE_TUNNEL, SAI_NEXT_HOP_ATTR_TUNNEL_ID);
    if (tunnel_obj == nullptr) {
        return SAI_STATUS_FAILURE;
    }
    attr.id = SAI_TUNNEL_ATTR_TYPE;
    CHECK_STATUS_W_MSG(tunnel_obj->get_attr(attr), "Missing SAI_TUNNEL_ATTR_TYPE in tunnel obj");

    if (attr.value.s32 == SAI_TUNNEL_TYPE_IPINIP) {
        return m_switch_db->m_tunnel_mgr_ipip.ipip_encap_nexthop_action(tunnel_nh_obj, tunnel_obj.get(), action);
    }

    if (attr.value.s32 != SAI_TUNNEL_TYPE_VXLAN) {
        SWSS_LOG_ERROR("Unsupported tunnel encap type %d in %s", attr.value.s32,
                        tunnel_obj->get_id().c_str());
        return SAI_STATUS_NOT_IMPLEMENTED;
    }

    if (action == Action::DELETE) {
        auto it = m_tunnel_encap_nexthop_map.find(object_id);
        if (it == m_tunnel_encap_nexthop_map.end()) {
            SWSS_LOG_ERROR("No VXLAN tunnel data for next hop %s",
                           tunnel_nh_obj->get_id().c_str());
            return SAI_STATUS_FAILURE;
        }
        sai_status_t status = remove_vxlan_encap_entries(it->second);
        if (status == SAI_STATUS_SUCCESS) {
            m_tunnel_encap_nexthop_map.erase(it);
        }
        return status;
    }
    if (action != Action::CREATE && action != Action::UPDATE) {
        return SAI_STATUS_NOT_IMPLEMENTED;
    }
    auto existing = m_tunnel_encap_nexthop_map.find(object_id);
    if (action == Action::CREATE && existing != m_tunnel_encap_nexthop_map.end()) {
        return SAI_STATUS_SUCCESS;
    }
    if (action == Action::UPDATE && existing == m_tunnel_encap_nexthop_map.end()) {
        return SAI_STATUS_FAILURE;
    }

    attr.id = SAI_TUNNEL_ATTR_ENCAP_SRC_IP;
    CHECK_STATUS_W_MSG(tunnel_obj->get_attr(attr), "Missing SAI_TUNNEL_ATTR_ENCAP_SRC_IP in tunnel obj");
    // SAI_TUNNEL_ATTR_ENCAP_TTL_MODE and SAI_TUNNEL_ATTR_ENCAP_TTL_VAL are not supported in vpp
    sai_ip_address_t src_ip = attr.value.ipaddr;

    attr.id = SAI_NEXT_HOP_ATTR_IP;
    CHECK_STATUS_W_MSG(tunnel_nh_obj->get_attr(attr), "Missing SAI_NEXT_HOP_ATTR_IP in %s", tunnel_nh_obj->get_id().c_str());
    sai_ip_address_t dst_ip = attr.value.ipaddr;

    uint32_t override_vni = 0;
    attr.id = SAI_NEXT_HOP_ATTR_TUNNEL_VNI;
    if (tunnel_nh_obj->get_attr(attr) == SAI_STATUS_SUCCESS) {
        override_vni = attr.value.u32;
    }

    std::array<uint8_t, 6> dst_mac = get_router_mac();
    attr.id = SAI_NEXT_HOP_ATTR_TUNNEL_MAC;
    if (tunnel_nh_obj->get_attr(attr) == SAI_STATUS_SUCCESS) {
        std::copy(attr.value.mac, attr.value.mac + dst_mac.size(), dst_mac.begin());
    }

    // SAI_TUNNEL_ATTR_PEER_MODE is CREATE_ONLY with a SAI default of P2MP.
    // decap_any (source-independent decap) is only correct for a P2MP tunnel;
    // a P2P tunnel has a single fixed remote peer (SAI marks ENCAP_DST_IP
    // validonly when PEER_MODE == P2P) and must keep exact outer-source
    // validation. Soft-read and default to P2MP so an omitted attribute
    // (get_attr returns SAI_STATUS_ITEM_NOT_FOUND) does not regress creation.
    bool tunnel_is_p2mp = true;
    attr.id = SAI_TUNNEL_ATTR_PEER_MODE;
    if (tunnel_obj->get_attr(attr) == SAI_STATUS_SUCCESS) {
        tunnel_is_p2mp = (attr.value.s32 == SAI_TUNNEL_PEER_MODE_P2MP);
    }

    std::map<uint32_t, std::shared_ptr<IpVrfInfo>> vni_to_vrf_map;
    if (override_vni != 0) {
        // The next-hop VNI replaces the mapper result. It does not create a
        // corresponding VNI-to-VRF decapsulation mapping.
        vni_to_vrf_map[override_vni] = nullptr;
    } else {
        auto mappers = tunnel_obj->get_linked_objects(
            SAI_OBJECT_TYPE_TUNNEL_MAP, SAI_TUNNEL_ATTR_ENCAP_MAPPERS);
        for (const auto& mapper : mappers) {
            attr.id = SAI_TUNNEL_MAP_ATTR_TYPE;
            CHECK_STATUS_W_MSG(mapper->get_attr(attr),
                               "Missing SAI_TUNNEL_MAP_ATTR_TYPE in %s",
                               mapper->get_id().c_str());
            if (attr.value.s32 != SAI_TUNNEL_MAP_TYPE_VIRTUAL_ROUTER_ID_TO_VNI) {
                continue;
            }
            auto entries = mapper->get_child_objs(SAI_OBJECT_TYPE_TUNNEL_MAP_ENTRY);
            if (!entries) {
                continue;
            }
            for (const auto& pair : *entries) {
                const auto& entry = pair.second;
                attr.id = SAI_TUNNEL_MAP_ENTRY_ATTR_VNI_ID_VALUE;
                CHECK_STATUS_W_MSG(entry->get_attr(attr),
                                   "Missing VNI in tunnel map entry %s",
                                   entry->get_id().c_str());
                uint32_t vni = attr.value.u32;
                attr.id = SAI_TUNNEL_MAP_ENTRY_ATTR_VIRTUAL_ROUTER_ID_KEY;
                CHECK_STATUS_W_MSG(entry->get_attr(attr),
                                   "Missing VRF in tunnel map entry %s",
                                   entry->get_id().c_str());
                if (action == Action::UPDATE && attr.value.oid != vrf_oid) {
                    continue;
                }
                auto vrf = m_switch_db->vpp_get_ip_vrf(attr.value.oid);
                if (!vrf || vni_to_vrf_map.count(vni)) {
                    SWSS_LOG_ERROR("Invalid or duplicate VNI %u in tunnel map entry %s",
                                   vni, entry->get_id().c_str());
                    return SAI_STATUS_FAILURE;
                }
                vni_to_vrf_map[vni] = vrf;
            }
        }
    }
    if (vni_to_vrf_map.empty()) {
        SWSS_LOG_ERROR("No VXLAN encapsulation VNI for next hop %s",
                       tunnel_nh_obj->get_id().c_str());
        return SAI_STATUS_FAILURE;
    }

    std::vector<VxlanEncapEntry> created;
    for (const auto& pair : vni_to_vrf_map) {
        if (existing != m_tunnel_encap_nexthop_map.end()) {
            auto cached = std::find_if(existing->second.begin(), existing->second.end(),
                [&](const VxlanEncapEntry& entry) { return entry.req.vni == pair.first; });
            if (cached != existing->second.end()) {
                if ((!cached->tunnel_data.ip_vrf && !pair.second) ||
                    (cached->tunnel_data.ip_vrf && pair.second &&
                     cached->tunnel_data.ip_vrf->m_obj_id == pair.second->m_obj_id)) {
                    continue;
                }
                SWSS_LOG_ERROR("VXLAN VNI %u already belongs to another VRF on next hop %s",
                               pair.first, tunnel_nh_obj->get_id().c_str());
                remove_vxlan_encap_entries(created);
                return SAI_STATUS_FAILURE;
            }
        }
        VxlanEncapEntry entry{};
        entry.req.dst_port = m_vxlan_port;
        entry.req.src_port = m_vxlan_port;
        entry.req.instance = ~0u;
        entry.req.decap_next_index = ~0u;
        entry.req.vni = pair.first;
        entry.req.decap_any = tunnel_is_p2mp && pair.second != nullptr;
        sai_ip_address_t_to_vpp_ip_addr_t(src_ip, entry.req.src_address);
        sai_ip_address_t_to_vpp_ip_addr_t(dst_ip, entry.req.dst_address);
        entry.tunnel_data.ip_vrf = pair.second;
        entry.tunnel_data.vni = pair.first;
        entry.dst_mac = dst_mac;

        if (create_vpp_vxlan_encap(entry.req, entry.tunnel_data, false,
                                   entry.dst_mac.data()) != SAI_STATUS_SUCCESS) {
            remove_vxlan_encap_entries(created);
            return SAI_STATUS_FAILURE;
        }
        if (pair.second) {
            if (create_vpp_vxlan_decap(entry.tunnel_data) != SAI_STATUS_SUCCESS) {
                remove_vpp_vxlan_encap(entry.req, entry.tunnel_data, false,
                                       entry.dst_mac.data());
                remove_vxlan_encap_entries(created);
                return SAI_STATUS_FAILURE;
            }
            entry.has_decap = true;
        }
        created.push_back(entry);
    }

    if (existing != m_tunnel_encap_nexthop_map.end()) {
        existing->second.insert(existing->second.end(), created.begin(), created.end());
    } else {
        m_tunnel_encap_nexthop_map.emplace(object_id, std::move(created));
    }
    return SAI_STATUS_SUCCESS;
}

sai_status_t
TunnelManager::remove_vxlan_encap_entries(
    _Inout_ std::vector<VxlanEncapEntry>& entries)
{
    SWSS_LOG_ENTER();

    sai_status_t result = SAI_STATUS_SUCCESS;
    for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
        if (it->has_decap && remove_vpp_vxlan_decap(it->tunnel_data) != SAI_STATUS_SUCCESS) {
            result = SAI_STATUS_FAILURE;
        }
        if (remove_vpp_vxlan_encap(it->req, it->tunnel_data, false,
                                   it->dst_mac.data()) != SAI_STATUS_SUCCESS) {
            result = SAI_STATUS_FAILURE;
        }
    }
    return result;
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
                    _Out_ TunnelVPPData& tunnel_data,
                    _In_  bool skip_neighbor,
                    _In_  const uint8_t* inner_dst_mac)
{
    SWSS_LOG_ENTER();

    int                         vpp_status;
    u_int32_t                   sw_if_index;
    char                        src_ip_str[INET6_ADDRSTRLEN];
    char                        dst_ip_str[INET6_ADDRSTRLEN];
    auto                        dst_mac = get_router_mac();
    if (inner_dst_mac) {
        std::copy(inner_dst_mac, inner_dst_mac + dst_mac.size(), dst_mac.begin());
    }
    sai_attribute_t             switch_mac_attr{};

    if (!skip_neighbor) {
        switch_mac_attr.id = SAI_SWITCH_ATTR_SRC_MAC_ADDRESS;
        sai_status_t status = m_switch_db->get(SAI_OBJECT_TYPE_SWITCH,
                                                m_switch_db->m_switch_id, 1,
                                                &switch_mac_attr);
        if (status != SAI_STATUS_SUCCESS) {
            SWSS_LOG_ERROR("Failed to read switch source MAC for VXLAN encapsulation: %d",
                           status);
            return status;
        }
    }

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

    if (!skip_neighbor) {
        /* the neighbour is to build inner ether. use no_fib_entry to avoid creating the nh in the fib, which will mess up underlay forwarding*/
        int neighbor_status;
        if (req.dst_address.sa_family == AF_INET6) {
            neighbor_status = ip6_nbr_add_del(NULL, sw_if_index,
                &req.dst_address.addr.ip6, false, true/*no_fib_entry*/, dst_mac.data(), 1);
        } else {
            neighbor_status = ip4_nbr_add_del(NULL, sw_if_index,
                &req.dst_address.addr.ip4, false, true/*no_fib_entry*/, dst_mac.data(), 1);
        }
        if (neighbor_status != 0) {
            SWSS_LOG_ERROR("Failed to set inner destination MAC on VXLAN tunnel sw_if %u",
                           sw_if_index);
            vpp_vxlan_tunnel_add_del(&req, 0, &sw_if_index);
            return SAI_STATUS_FAILURE;
        }
        // VPP uses the tunnel interface MAC as the inner Ethernet source.
        // The VXLAN default router MAC is the inner destination, not source.
        if (sw_interface_set_mac_by_index(sw_if_index, switch_mac_attr.value.mac) != 0) {
            SWSS_LOG_ERROR("Failed to set switch MAC on VXLAN tunnel sw_if %u",
                           sw_if_index);
            if (req.dst_address.sa_family == AF_INET6) {
                ip6_nbr_add_del(NULL, sw_if_index, &req.dst_address.addr.ip6, false,
                                true, dst_mac.data(), 0);
            } else {
                ip4_nbr_add_del(NULL, sw_if_index, &req.dst_address.addr.ip4, false,
                                true, dst_mac.data(), 0);
            }
            vpp_vxlan_tunnel_add_del(&req, 0, &sw_if_index);
            return SAI_STATUS_FAILURE;
        }
    }

    SWSS_LOG_INFO("successfully created encap for vxlan tunnel %d", sw_if_index);
    return SAI_STATUS_SUCCESS;
}

sai_status_t
TunnelManager::remove_vpp_vxlan_encap(
                    _In_  vpp_vxlan_tunnel_t& req,
                    _In_ TunnelVPPData& tunnel_data,
                    _In_  bool skip_neighbor,
                    _In_  const uint8_t* inner_dst_mac)
{
    SWSS_LOG_ENTER();

    int                         vpp_status;
    u_int32_t                   sw_if_index = tunnel_data.sw_if_index;
    char                        src_ip_str[INET6_ADDRSTRLEN];
    char                        dst_ip_str[INET6_ADDRSTRLEN];
    auto                        dst_mac = get_router_mac();
    if (inner_dst_mac) {
        std::copy(inner_dst_mac, inner_dst_mac + dst_mac.size(), dst_mac.begin());
    }

    if (!skip_neighbor) {
        if (req.dst_address.sa_family == AF_INET6) {
            ip6_nbr_add_del(NULL, tunnel_data.sw_if_index, &req.dst_address.addr.ip6, false, true/*no_fib_entry*/, dst_mac.data(), 0);
        } else {
            ip4_nbr_add_del(NULL, tunnel_data.sw_if_index, &req.dst_address.addr.ip4, false, true/*no_fib_entry*/, dst_mac.data(), 0);
        }
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
    snprintf(hw_bvi_ifname, sizeof(hw_bvi_ifname), "bvi%u", tunnel_data.bd_id);
    //create bvi interface using instance same as bd_id
    vpp_status = create_bvi_interface(bvi_mac, bd_id);
    if (vpp_status != 0) {
        SWSS_LOG_ERROR("Failed to create bvi interface");
        m_switch_db->dynamic_bd_id_pool.free(bd_id);
        return SAI_STATUS_FAILURE;
    }
    bool bridge_created = false;
    auto rollback = [&]() -> sai_status_t {
        delete_bvi_interface(hw_bvi_ifname);
        if (bridge_created) {
            vpp_bridge_domain_add_del(tunnel_data.bd_id, false);
        }
        m_switch_db->dynamic_bd_id_pool.free(bd_id);
        refresh_interfaces_list();
        return SAI_STATUS_FAILURE;
    };
    // Get new list of physical interfaces from VS
    refresh_interfaces_list();

    //bring up bvi interface
    vpp_status = interface_set_state(hw_bvi_ifname, true);
    if (vpp_status != 0) {
        SWSS_LOG_ERROR("Failed to bring up bvi interface");
        return rollback();
    }

    //Create bridge and set BVI to the BD
    vpp_status = set_sw_interface_l2_bridge(hw_bvi_ifname, bd_id, true, VPP_API_PORT_TYPE_BVI);
    if (vpp_status != 0) {
        SWSS_LOG_ERROR("Failed to add bvi interface to bd");
        return rollback();
    }
    bridge_created = true;

    //bind bvi to vrf
    vpp_status = set_interface_vrf(hw_bvi_ifname, 0, tunnel_data.ip_vrf->m_vrf_id, tunnel_data.ip_vrf->m_is_ipv6);
    if (vpp_status != 0) {
        SWSS_LOG_ERROR("Failed to bind VXLAN bvi interface to vrf");
        return rollback();
    }

    //set bvi IPv4
    uint16_t offset = (uint16_t)((uint16_t)(bd_id - SwitchVpp::dynamic_bd_id_base) + 2);

    bvi_ip_prefix.prefix_len = 32;
    bvi_ip_prefix.prefix_addr.sa_family = AF_INET;
    struct sockaddr_in *sin =  &bvi_ip_prefix.prefix_addr.addr.ip4;
    sin->sin_addr.s_addr = htonl(offset);
    vpp_status = interface_ip_address_add_del(hw_bvi_ifname, &bvi_ip_prefix, true);
    if (vpp_status != 0) {
        SWSS_LOG_ERROR("Failed to config IP on bvi interface");
        return rollback();
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
        return rollback();
    }

    //set vxlan tunnel to bridge domain
    vpp_status = set_sw_interface_l2_bridge_by_index(tunnel_if_index, bd_id, true, VPP_API_PORT_TYPE_NORMAL);
    if (vpp_status != 0) {
        SWSS_LOG_ERROR("Failed to add tunnel interface to bd");
        return rollback();
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

    // Detach the vxlan tunnel interface from the BD before deleting the BD. The
    // tunnel itself is deleted later by remove_vpp_vxlan_encap; if it is still a
    // member here, vpp_bridge_domain_add_del(is_add=0) fails with -120 (BD in use).
    if (set_sw_interface_l2_bridge_by_index(tunnel_data.sw_if_index, tunnel_data.bd_id,
                                            false, VPP_API_PORT_TYPE_NORMAL) != 0) {
        SWSS_LOG_ERROR("VXLAN decap remove: failed to detach tunnel sw_if %u from BD %u; "
                       "subsequent BD delete may fail with -120 (BD in use)",
                       tunnel_data.sw_if_index, tunnel_data.bd_id);
    }

    m_switch_db->dynamic_bd_id_pool.free(tunnel_data.bd_id);
    refresh_interfaces_list();
    //bd is create automatically when the fist interface is add to it but requires manual deletion
    vpp_bridge_domain_add_del(tunnel_data.bd_id, false);
    SWSS_LOG_INFO("successfully deleted decap of vxlan tunnel %d with BD %d",
                        tunnel_data.sw_if_index, tunnel_data.bd_id);
    return SAI_STATUS_SUCCESS;
}

sai_status_t
TunnelManager::create_l2_vxlan_tunnel_for_vni(
    _In_ sai_ip_address_t src_ip,
    _In_ sai_ip_address_t dst_ip,
    _In_ uint32_t vni,
    _In_ uint16_t vlan_id,
    _Out_ uint32_t& sw_if_index,
    _In_ sai_object_id_t tunnel_term_oid,
    _In_ sai_object_id_t tunnel_map_entry_oid)
{
    SWSS_LOG_ENTER();

    sw_if_index = ~0;

    auto existing_it = m_l2_tunnel_map.find(vni);
    if (existing_it != m_l2_tunnel_map.end()) {
        const TunnelVPPData& existing = existing_it->second;

        // Preserve the existing EVPN behavior for multiple P2P tunnel
        // objects that reference the same VNI. m_l2_tunnel_map predates the
        // term-owned path and can represent only one of those objects; making
        // that limitation a hard error here would reject control-plane state
        // that was previously accepted. The stricter comparison below is for
        // the new term-owned path, where silently reusing a different match
        // would claim that a SAI termination entry had been implemented when
        // it had not.
        const bool existing_term_owned =
            existing.tunnel_term_oid != SAI_NULL_OBJECT_ID;
        const bool requested_term_owned =
            tunnel_term_oid != SAI_NULL_OBJECT_ID;

        if (!existing_term_owned && !requested_term_owned) {
            SWSS_LOG_NOTICE("VNI %u already has an object-owned L2 VXLAN "
                            "tunnel, preserving the existing one", vni);
            sw_if_index = existing.sw_if_index;
            return SAI_STATUS_SUCCESS;
        }

        const bool same_match =
            existing.vlan_id == vlan_id &&
            sai_ip_address_equal(existing.src_ip, src_ip) &&
            sai_ip_address_equal(existing.dst_ip, dst_ip);

        // Term-owned tunnels can only be reused by the same term and map entry.
        // Sharing with another SAI object would let either owner remove the
        // VPP tunnel while the other still needs it.
        const bool owner_compatible =
            existing_term_owned && requested_term_owned &&
            existing.tunnel_term_oid == tunnel_term_oid &&
            existing.tunnel_map_entry_oid == tunnel_map_entry_oid;

        if (!same_match || !owner_compatible) {
            SWSS_LOG_ERROR("VNI %u already has an incompatible L2 VXLAN "
                           "match/owner (existing term=%s, requested term=%s)",
                           vni,
                           sai_serialize_object_id(existing.tunnel_term_oid).c_str(),
                           sai_serialize_object_id(tunnel_term_oid).c_str());
            return SAI_STATUS_NOT_SUPPORTED;
        }

        SWSS_LOG_NOTICE("VNI %u already has the requested L2 VXLAN match", vni);
        sw_if_index = existing.sw_if_index;
        return SAI_STATUS_SUCCESS;
    }

    vpp_vxlan_tunnel_t req;
    TunnelVPPData tunnel_data;

    memset(&req, 0, sizeof(req));
    req.vni = vni;
    req.src_port = m_vxlan_port;
    req.dst_port = m_vxlan_port;
    req.instance = ~0;
    req.decap_next_index = ~0;
    sai_ip_address_t_to_vpp_ip_addr_t(src_ip, req.src_address);
    sai_ip_address_t_to_vpp_ip_addr_t(dst_ip, req.dst_address);

    tunnel_data.vni = vni;
    tunnel_data.src_ip = src_ip;
    tunnel_data.dst_ip = dst_ip;
    tunnel_data.vlan_id = vlan_id;
    tunnel_data.ip_vrf = nullptr;
    tunnel_data.tunnel_term_oid = tunnel_term_oid;
    tunnel_data.tunnel_map_entry_oid = tunnel_map_entry_oid;

    if (create_vpp_vxlan_encap(req, tunnel_data, /*skip_neighbor=*/true) != SAI_STATUS_SUCCESS) {
        SWSS_LOG_ERROR("Failed to create VPP VXLAN tunnel for VNI=%u", vni);
        return SAI_STATUS_FAILURE;
    }

    if (tunnel_term_oid != SAI_NULL_OBJECT_ID) {
        // An exact P2P term permits decap, but does not authorize bridge
        // traffic to use this tunnel for encapsulation.
        uint8_t mask[16] = {0xff};
        uint32_t table_index = ~0u;
        if (vpp_classify_table_create(8, 4 * 1024, 0, 1, ~0u,
                                      0 /* output miss: drop */, mask,
                                      sizeof(mask), &table_index) != 0) {
            remove_vpp_vxlan_encap(req, tunnel_data, /*skip_neighbor=*/true);
            return SAI_STATUS_FAILURE;
        }
        tunnel_data.l2_output_drop_table_index = table_index;
        if (vpp_classify_set_interface_l2_tables_by_index(
                tunnel_data.sw_if_index, table_index, table_index,
                table_index, false /* output */) != 0) {
            vpp_classify_table_delete(table_index);
            remove_vpp_vxlan_encap(req, tunnel_data, /*skip_neighbor=*/true);
            return SAI_STATUS_FAILURE;
        }
    }

    int vpp_status = set_sw_interface_l2_bridge_by_index(
        tunnel_data.sw_if_index, vlan_id, true, VPP_API_PORT_TYPE_NORMAL);
    if (vpp_status != 0) {
        SWSS_LOG_ERROR("Failed to add tunnel sw_if %u to BD %u",
            tunnel_data.sw_if_index, vlan_id);
        if (tunnel_data.l2_output_drop_table_index != ~0u) {
            if (vpp_classify_set_interface_l2_tables_by_index(
                    tunnel_data.sw_if_index, ~0u, ~0u, ~0u,
                    false /* output */) == 0) {
                vpp_classify_table_delete(tunnel_data.l2_output_drop_table_index);
            }
        }
        remove_vpp_vxlan_encap(req, tunnel_data, /*skip_neighbor=*/true);
        return SAI_STATUS_FAILURE;
    }

    // A term-owned tunnel is decap-only, so it has nothing to learn. VPP also
    // drops a frame whose source MAC is static on another bridge port, such
    // as a decapsulated frame sourced from the VLAN BVI MAC. Entering bridge
    // mode enables learning, so this must follow the bridge attach.
    if (tunnel_term_oid != SAI_NULL_OBJECT_ID &&
        set_l2_interface_flags_by_index(tunnel_data.sw_if_index,
                                        VPP_BD_FLAG_LEARN, false) != 0) {
        SWSS_LOG_ERROR("Failed to disable learning on L2 VXLAN tunnel sw_if %u",
                       tunnel_data.sw_if_index);
        set_sw_interface_l2_bridge_by_index(
            tunnel_data.sw_if_index, vlan_id, false, VPP_API_PORT_TYPE_NORMAL);
        if (vpp_classify_set_interface_l2_tables_by_index(
                tunnel_data.sw_if_index, ~0u, ~0u, ~0u,
                false /* output */) == 0) {
            vpp_classify_table_delete(tunnel_data.l2_output_drop_table_index);
        }
        remove_vpp_vxlan_encap(req, tunnel_data, /*skip_neighbor=*/true);
        return SAI_STATUS_FAILURE;
    }

    m_l2_tunnel_map[vni] = tunnel_data;
    sw_if_index = tunnel_data.sw_if_index;

    char src_str[INET6_ADDRSTRLEN], dst_str[INET6_ADDRSTRLEN];
    vpp_ip_addr_t_to_string(&req.src_address, src_str, sizeof(src_str));
    vpp_ip_addr_t_to_string(&req.dst_address, dst_str, sizeof(dst_str));

    SWSS_LOG_NOTICE("Created L2 VXLAN: src=%s dst=%s VNI=%u VLAN=%u sw_if=%u",
        src_str, dst_str, vni, vlan_id, sw_if_index);

    return SAI_STATUS_SUCCESS;
}

sai_status_t
TunnelManager::rollback_l2_vxlan_tunnels(
    _In_ const std::vector<uint32_t>& created_vnis)
{
    SWSS_LOG_ENTER();

    sai_status_t rollback_status = SAI_STATUS_SUCCESS;
    for (auto it = created_vnis.rbegin(); it != created_vnis.rend(); ++it) {
        sai_status_t status = remove_l2_vxlan_tunnel_for_vni(*it);
        if (status != SAI_STATUS_SUCCESS) {
            SWSS_LOG_ERROR("Failed to roll back L2 VXLAN VNI %u: status=%d",
                           *it, status);
            rollback_status = status;
        }
    }
    return rollback_status;
}

sai_status_t
TunnelManager::create_l2_vxlan_tunnels_for_object(
    _In_ const SaiObject* tunnel_obj,
    _In_ sai_ip_address_t local_ip,
    _In_ sai_ip_address_t remote_ip,
    _In_ sai_object_id_t tunnel_term_oid,
    _Out_ uint32_t& sw_if_index,
    _Out_ std::vector<uint32_t>* new_vnis)
{
    SWSS_LOG_ENTER();

    sw_if_index = ~0;
    if (new_vnis) {
        new_vnis->clear();
    }
    sai_attribute_t attr;
    std::vector<uint32_t> created_vnis;

    auto decap_mappers = tunnel_obj->get_linked_objects(
        SAI_OBJECT_TYPE_TUNNEL_MAP, SAI_TUNNEL_ATTR_DECAP_MAPPERS);

    for (auto mapper : decap_mappers) {
        attr.id = SAI_TUNNEL_MAP_ATTR_TYPE;
        if (mapper->get_attr(attr) != SAI_STATUS_SUCCESS ||
            attr.value.s32 != SAI_TUNNEL_MAP_TYPE_VNI_TO_VLAN_ID) {
            continue;
        }

        auto entries = mapper->get_child_objs(SAI_OBJECT_TYPE_TUNNEL_MAP_ENTRY);
        if (!entries) {
            continue;
        }

        for (auto& entry_pair : *entries) {
            auto entry = entry_pair.second;
            uint32_t vni = 0;
            uint16_t vlan_id = 0;
            sai_object_id_t map_entry_oid;
            sai_deserialize_object_id(entry_pair.first, map_entry_oid);

            attr.id = SAI_TUNNEL_MAP_ENTRY_ATTR_VNI_ID_KEY;
            if (entry->get_attr(attr) == SAI_STATUS_SUCCESS) {
                vni = attr.value.u32;
            }

            attr.id = SAI_TUNNEL_MAP_ENTRY_ATTR_VLAN_ID_VALUE;
            if (entry->get_attr(attr) == SAI_STATUS_SUCCESS) {
                vlan_id = attr.value.u16;
            }

            if (vni == 0 || vlan_id == 0) {
                continue;
            }

            const bool already_present =
                m_l2_tunnel_map.find(vni) != m_l2_tunnel_map.end();
            uint32_t vni_sw_if_index;
            sai_status_t status = create_l2_vxlan_tunnel_for_vni(
                local_ip, remote_ip, vni, vlan_id, vni_sw_if_index,
                tunnel_term_oid, map_entry_oid);
            if (status != SAI_STATUS_SUCCESS) {
                sai_status_t rollback_status = rollback_l2_vxlan_tunnels(created_vnis);
                if (rollback_status != SAI_STATUS_SUCCESS) {
                    return rollback_status;
                }
                return status;
            }
            if (!already_present &&
                m_l2_tunnel_map.find(vni) != m_l2_tunnel_map.end()) {
                created_vnis.push_back(vni);
            }

            if (sw_if_index == (uint32_t)~0) {
                sw_if_index = vni_sw_if_index;
            }
        }
    }

    if (new_vnis) {
        *new_vnis = std::move(created_vnis);
    }
    return SAI_STATUS_SUCCESS;
}

sai_status_t
TunnelManager::create_l2_vxlan_tunnel(
    _In_ sai_object_id_t tunnel_oid,
    _Out_ uint32_t& sw_if_index)
{
    SWSS_LOG_ENTER();

    sw_if_index = ~0;

    // Get tunnel object
    auto tunnel_obj = m_switch_db->get_sai_object(SAI_OBJECT_TYPE_TUNNEL,
        sai_serialize_object_id(tunnel_oid));
    if (!tunnel_obj) {
        SWSS_LOG_ERROR("Tunnel %s not found", sai_serialize_object_id(tunnel_oid).c_str());
        return SAI_STATUS_FAILURE;
    }

    // Check tunnel type
    sai_attribute_t attr;
    attr.id = SAI_TUNNEL_ATTR_TYPE;
    if (tunnel_obj->get_attr(attr) != SAI_STATUS_SUCCESS) {
        SWSS_LOG_ERROR("Missing SAI_TUNNEL_ATTR_TYPE");
        return SAI_STATUS_FAILURE;
    }
    if (attr.value.s32 != SAI_TUNNEL_TYPE_VXLAN) {
        SWSS_LOG_NOTICE("Not a VXLAN tunnel (type=%d), skipping", attr.value.s32);
        return SAI_STATUS_SUCCESS;
    }

    // Get src IP
    attr.id = SAI_TUNNEL_ATTR_ENCAP_SRC_IP;
    if (tunnel_obj->get_attr(attr) != SAI_STATUS_SUCCESS) {
        SWSS_LOG_ERROR("Missing ENCAP_SRC_IP");
        return SAI_STATUS_FAILURE;
    }
    sai_ip_address_t src_ip = attr.value.ipaddr;

    // Only an explicit ENCAP_DST_IP makes the tunnel object itself authoritative
    // for an exact remote peer (the EVPN P2P path). Static SONiC tunnels can omit
    // ENCAP_DST_IP even when their subsequently-created termination entry is
    // P2P, so defer those until create_l2_vxlan_tunnel_term sees the term.
    attr.id = SAI_TUNNEL_ATTR_ENCAP_DST_IP;
    if (tunnel_obj->get_attr(attr) != SAI_STATUS_SUCCESS) {
        SWSS_LOG_NOTICE("Tunnel %s has no ENCAP_DST_IP; waiting for its "
                        "termination entry before programming L2 decap",
                        sai_serialize_object_id(tunnel_oid).c_str());
        return SAI_STATUS_SUCCESS;
    }
    sai_ip_address_t dst_ip = attr.value.ipaddr;

    sai_status_t status = create_l2_vxlan_tunnels_for_object(
        tunnel_obj.get(), src_ip, dst_ip, SAI_NULL_OBJECT_ID, sw_if_index);
    if (status != SAI_STATUS_SUCCESS) {
        return status;
    }

    if (sw_if_index == (uint32_t)~0) {
        SWSS_LOG_NOTICE("No VNI-to-VLAN mappings found for tunnel %s. "
            "Not an L2 VXLAN tunnel, skipping.",
            sai_serialize_object_id(tunnel_oid).c_str());
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t
TunnelManager::create_l2_vxlan_tunnel_term(
    _In_ const std::string& serializedObjectId,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list,
    _Out_ std::vector<uint32_t>& created_vnis)
{
    SWSS_LOG_ENTER();

    created_vnis.clear();
    SaiCachedObject term_obj(m_switch_db, SAI_OBJECT_TYPE_TUNNEL_TERM_TABLE_ENTRY,
                             serializedObjectId, attr_count, attr_list);

    sai_attribute_t attr;
    attr.id = SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_TYPE;
    if (term_obj.get_attr(attr) != SAI_STATUS_SUCCESS ||
        attr.value.s32 != SAI_TUNNEL_TERM_TABLE_ENTRY_TYPE_P2P) {
        return SAI_STATUS_SUCCESS;
    }

    attr.id = SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_ACTION_TUNNEL_ID;
    if (term_obj.get_attr(attr) != SAI_STATUS_SUCCESS) {
        SWSS_LOG_ERROR("VXLAN term %s is missing ACTION_TUNNEL_ID",
                       serializedObjectId.c_str());
        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }
    sai_object_id_t tunnel_oid = attr.value.oid;

    auto tunnel_obj = m_switch_db->get_sai_object(
        SAI_OBJECT_TYPE_TUNNEL, sai_serialize_object_id(tunnel_oid));
    if (!tunnel_obj) {
        SWSS_LOG_ERROR("VXLAN term %s references missing tunnel %s",
                       serializedObjectId.c_str(),
                       sai_serialize_object_id(tunnel_oid).c_str());
        return SAI_STATUS_FAILURE;
    }

    attr.id = SAI_TUNNEL_ATTR_TYPE;
    if (tunnel_obj->get_attr(attr) != SAI_STATUS_SUCCESS ||
        attr.value.s32 != SAI_TUNNEL_TYPE_VXLAN) {
        return SAI_STATUS_INVALID_PARAMETER;
    }

    bool has_l2_decap_mapper = false;
    auto decap_mappers = tunnel_obj->get_linked_objects(
        SAI_OBJECT_TYPE_TUNNEL_MAP, SAI_TUNNEL_ATTR_DECAP_MAPPERS);
    for (const auto& mapper : decap_mappers) {
        attr.id = SAI_TUNNEL_MAP_ATTR_TYPE;
        if (mapper->get_attr(attr) == SAI_STATUS_SUCCESS &&
            attr.value.s32 == SAI_TUNNEL_MAP_TYPE_VNI_TO_VLAN_ID) {
            has_l2_decap_mapper = true;
            break;
        }
    }
    if (!has_l2_decap_mapper) {
        return SAI_STATUS_SUCCESS;
    }

    VxlanTermMatch match;
    bool applicable = false;
    sai_status_t status = get_exact_vxlan_term_match(
        &term_obj, match, applicable);
    if (status != SAI_STATUS_SUCCESS) {
        return status;
    }
    if (!applicable) {
        return SAI_STATUS_SUCCESS;
    }

    // ENCAP_DST_IP identifies the existing EVPN/P2P path. Preserve its
    // object-driven behavior; this handler is only for static tunnels whose
    // exact remote endpoint exists solely on the termination entry.
    attr.id = SAI_TUNNEL_ATTR_ENCAP_DST_IP;
    if (tunnel_obj->get_attr(attr) == SAI_STATUS_SUCCESS) {
        return SAI_STATUS_SUCCESS;
    }

    sai_object_id_t term_oid;
    sai_deserialize_object_id(serializedObjectId, term_oid);

    uint32_t sw_if_index;
    status = create_l2_vxlan_tunnels_for_object(
        tunnel_obj.get(), match.local_ip, match.remote_ip,
        term_oid, sw_if_index, &created_vnis);
    if (status != SAI_STATUS_SUCCESS) {
        // The helper rolls back exactly the VNIs created by this attempt.
        // Compatible VNIs that existed before it must remain untouched.
        return status;
    }

    if (sw_if_index == (uint32_t)~0) {
        SWSS_LOG_NOTICE("VXLAN term %s has no VNI-to-VLAN mappings yet; "
                        "deferring L2 decap creation",
                        serializedObjectId.c_str());
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t
TunnelManager::install_l3_vxlan_decap_terms(
    _In_ const std::string& map_entry_serialized_oid,
    _In_ uint32_t vni,
    _In_ sai_object_id_t tunnel_map_oid,
    _In_ const SaiObject* map_entry_obj)
{
    SWSS_LOG_ENTER();

    // Best-effort / idempotent by design: this is called both when the decap
    // map entry is created and from the late-tunnel hook when a tunnel that
    // references the mapper appears later, so a legitimately-incomplete
    // intermediate state (mapper present but no tunnel yet, or vice-versa) is
    // expected and must not fail the caller. Every early return here therefore
    // yields SAI_STATUS_SUCCESS; the genuine-error paths (no VRF, per-tunnel
    // decap create failure) log SWSS_LOG_ERROR and are skipped rather than
    // propagated, because the term is re-attempted on the next hook and a hard
    // failure would abort an otherwise valid tunnel-map-entry create.
    if (vni == 0 || tunnel_map_oid == SAI_NULL_OBJECT_ID) {
        return SAI_STATUS_SUCCESS;
    }

    sai_object_id_t term_oid;
    sai_deserialize_object_id(map_entry_serialized_oid, term_oid);

    // Per-(map entry, VTEP) idempotency. The decap terms recorded under this
    // map entry are per-tunnel (one per VTEP src IP), so we must NOT early-
    // return on the entry as a whole: a tunnel created AFTER this entry was
    // first processed (e.g. a second VTEP tunnel referencing the same mapper)
    // still needs its own decap term. Only VTEPs already recorded are skipped
    // in the loop below; a newly-appearing VTEP is installed and appended to
    // the entry's vector.
    auto existing_it = m_vxlan_decap_term_map.find(term_oid);
    const std::vector<TunnelVPPData>* existing =
        (existing_it != m_vxlan_decap_term_map.end()) ? &existing_it->second : nullptr;

    // The map entry VR -> VPP VRF that routes the decapped inner packet.
    sai_attribute_t attr;
    attr.id = SAI_TUNNEL_MAP_ENTRY_ATTR_VIRTUAL_ROUTER_ID_VALUE;
    if (map_entry_obj->get_attr(attr) != SAI_STATUS_SUCCESS) {
        return SAI_STATUS_SUCCESS;
    }
    auto ip_vrf = m_switch_db->vpp_get_ip_vrf(attr.value.oid);
    if (!ip_vrf) {
        SWSS_LOG_ERROR("VXLAN decap map entry %s: no VRF for VNI %u",
                       map_entry_serialized_oid.c_str(), vni);
        return SAI_STATUS_SUCCESS;
    }

    // Find the VXLAN tunnel(s) referencing this decap mapper; each tunnel's
    // ENCAP_SRC_IP is the local VTEP IP to make decappable.
    auto mapper_obj = m_switch_db->get_sai_object(SAI_OBJECT_TYPE_TUNNEL_MAP,
        sai_serialize_object_id(tunnel_map_oid));
    if (!mapper_obj) {
        return SAI_STATUS_SUCCESS;
    }
    auto tunnels = mapper_obj->get_child_objs(SAI_OBJECT_TYPE_TUNNEL);
    if (!tunnels) {
        return SAI_STATUS_SUCCESS;
    }

    std::vector<TunnelVPPData> created;

    // A VTEP already has a decap term if it is recorded under this entry from a
    // prior hook (existing) or was installed earlier in this same loop pass.
    auto vtep_already_termed = [](const std::vector<TunnelVPPData>* vec,
                                  const sai_ip_address_t& vtep) -> bool {
        if (!vec) return false;
        for (const auto& t : *vec) {
            if (sai_ip_address_equal(t.src_ip, vtep)) return true;
        }
        return false;
    };

    for (auto& tunnel_pair : *tunnels) {
        auto tunnel_obj = tunnel_pair.second;

        attr.id = SAI_TUNNEL_ATTR_TYPE;
        if (tunnel_obj->get_attr(attr) != SAI_STATUS_SUCCESS ||
            attr.value.s32 != SAI_TUNNEL_TYPE_VXLAN) continue;

        attr.id = SAI_TUNNEL_ATTR_ENCAP_SRC_IP;
        if (tunnel_obj->get_attr(attr) != SAI_STATUS_SUCCESS) continue;
        sai_ip_address_t vtep_ip = attr.value.ipaddr;

        // A P2P tunnel requires exact outer-source matching, not this
        // source-independent decap path. The SAI default is P2MP.
        attr.id = SAI_TUNNEL_ATTR_PEER_MODE;
        if (tunnel_obj->get_attr(attr) == SAI_STATUS_SUCCESS &&
            attr.value.s32 != SAI_TUNNEL_PEER_MODE_P2MP) continue;

        // Skip VTEPs that already have a decap term under this map entry so a
        // newly-appearing tunnel still gets one (per-VTEP idempotency).
        if (vtep_already_termed(existing, vtep_ip) ||
            vtep_already_termed(&created, vtep_ip)) {
            continue;
        }

        TunnelVPPData td;
        if (create_vxlan_decap_term(vtep_ip, vni, ip_vrf, td)
                != SAI_STATUS_SUCCESS) {
            SWSS_LOG_ERROR("VXLAN decap map entry %s: decap create failed VNI %u",
                           map_entry_serialized_oid.c_str(), vni);
            continue;
        }
        created.push_back(td);
    }

    if (!created.empty()) {
        auto& vec = m_vxlan_decap_term_map[term_oid];
        vec.insert(vec.end(), created.begin(), created.end());
    }
    return SAI_STATUS_SUCCESS;
}

sai_status_t
TunnelManager::handle_l3_vxlan_tunnel_create(
    _In_ sai_object_id_t tunnel_oid)
{
    SWSS_LOG_ENTER();

    // Late-tunnel hook (M3): a TUNNEL_MAP_ENTRY of type VNI_TO_VIRTUAL_ROUTER_ID
    // may be created before the TUNNEL that references its mapper. In that case
    // handle_l2_vxlan_tunnel_map_entry saw no tunnels and installed nothing. Now
    // that the tunnel exists, rescan its L3 decap mappers and install any decap
    // terms still missing. Idempotent via m_vxlan_decap_term_map.
    auto tunnel_obj = m_switch_db->get_sai_object(SAI_OBJECT_TYPE_TUNNEL,
        sai_serialize_object_id(tunnel_oid));
    if (!tunnel_obj) {
        return SAI_STATUS_SUCCESS;
    }

    sai_attribute_t attr;
    attr.id = SAI_TUNNEL_ATTR_TYPE;
    if (tunnel_obj->get_attr(attr) != SAI_STATUS_SUCCESS ||
        attr.value.s32 != SAI_TUNNEL_TYPE_VXLAN) {
        return SAI_STATUS_SUCCESS;
    }

    auto decap_mappers = tunnel_obj->get_linked_objects(
        SAI_OBJECT_TYPE_TUNNEL_MAP, SAI_TUNNEL_ATTR_DECAP_MAPPERS);

    for (auto mapper : decap_mappers) {
        attr.id = SAI_TUNNEL_MAP_ATTR_TYPE;
        if (mapper->get_attr(attr) != SAI_STATUS_SUCCESS) continue;
        if (attr.value.s32 != SAI_TUNNEL_MAP_TYPE_VNI_TO_VIRTUAL_ROUTER_ID) continue;

        sai_object_id_t mapper_oid;
        sai_deserialize_object_id(mapper->get_id(), mapper_oid);

        auto entries = mapper->get_child_objs(SAI_OBJECT_TYPE_TUNNEL_MAP_ENTRY);
        if (!entries) continue;

        for (auto& entry_pair : *entries) {
            auto entry = entry_pair.second;

            uint32_t vni = 0;
            attr.id = SAI_TUNNEL_MAP_ENTRY_ATTR_VNI_ID_KEY;
            if (entry->get_attr(attr) == SAI_STATUS_SUCCESS) {
                vni = attr.value.u32;
            }

            install_l3_vxlan_decap_terms(entry->get_id(), vni, mapper_oid,
                                         entry.get());
        }
    }

    return SAI_STATUS_SUCCESS;
}


sai_status_t
TunnelManager::handle_l3_vxlan_tunnel_removal(
    _In_ sai_object_id_t tunnel_oid)
{
    SWSS_LOG_ENTER();

    // Defense in depth (mirror of handle_l3_vxlan_tunnel_create): sweep the L3
    // VNET decap terms this tunnel's VTEP owns. Normal teardown removes the
    // TUNNEL_MAP_ENTRYs first and handle_l2_vxlan_tunnel_map_entry_removal frees
    // the terms; this covers the case where the TUNNEL is deleted while its map
    // entries are kept. Runs before remove_internal, so the tunnel and its
    // DECAP_MAPPERS links are still in the DB.
    auto tunnel_obj = m_switch_db->get_sai_object(SAI_OBJECT_TYPE_TUNNEL,
        sai_serialize_object_id(tunnel_oid));
    if (!tunnel_obj) {
        return SAI_STATUS_SUCCESS;
    }

    sai_attribute_t attr;
    attr.id = SAI_TUNNEL_ATTR_TYPE;
    if (tunnel_obj->get_attr(attr) != SAI_STATUS_SUCCESS ||
        attr.value.s32 != SAI_TUNNEL_TYPE_VXLAN) {
        return SAI_STATUS_SUCCESS;
    }

    // The VTEP whose decap terms this tunnel owns is its ENCAP_SRC_IP.
    attr.id = SAI_TUNNEL_ATTR_ENCAP_SRC_IP;
    if (tunnel_obj->get_attr(attr) != SAI_STATUS_SUCCESS) {
        return SAI_STATUS_SUCCESS;
    }
    sai_ip_address_t deleted_vtep = attr.value.ipaddr;

    // Refcount guard: install_l3_vxlan_decap_terms deduplicates one term per
    // (map entry, VTEP src) across all tunnels that reference the mapper, so the
    // term must survive as long as any OTHER VXLAN tunnel still references this
    // mapper with the same VTEP source. The tunnel being deleted is still linked
    // at this point, so exclude it by OID.
    auto vtep_still_referenced =
        [&](const std::shared_ptr<SaiDBObject>& mapper) -> bool {
        auto tunnels = mapper->get_child_objs(SAI_OBJECT_TYPE_TUNNEL);
        if (!tunnels) return false;
        for (auto& tp : *tunnels) {
            auto t = tp.second;
            sai_object_id_t t_oid;
            sai_deserialize_object_id(t->get_id(), t_oid);
            if (t_oid == tunnel_oid) continue;
            sai_attribute_t a;
            a.id = SAI_TUNNEL_ATTR_TYPE;
            if (t->get_attr(a) != SAI_STATUS_SUCCESS ||
                a.value.s32 != SAI_TUNNEL_TYPE_VXLAN) continue;
            a.id = SAI_TUNNEL_ATTR_PEER_MODE;
            if (t->get_attr(a) == SAI_STATUS_SUCCESS &&
                a.value.s32 != SAI_TUNNEL_PEER_MODE_P2MP) continue;
            a.id = SAI_TUNNEL_ATTR_ENCAP_SRC_IP;
            if (t->get_attr(a) != SAI_STATUS_SUCCESS) continue;
            if (sai_ip_address_equal(a.value.ipaddr, deleted_vtep)) return true;
        }
        return false;
    };

    auto decap_mappers = tunnel_obj->get_linked_objects(
        SAI_OBJECT_TYPE_TUNNEL_MAP, SAI_TUNNEL_ATTR_DECAP_MAPPERS);

    for (auto mapper : decap_mappers) {
        attr.id = SAI_TUNNEL_MAP_ATTR_TYPE;
        if (mapper->get_attr(attr) != SAI_STATUS_SUCCESS) continue;
        if (attr.value.s32 != SAI_TUNNEL_MAP_TYPE_VNI_TO_VIRTUAL_ROUTER_ID) continue;

        // A surviving tunnel with the same VTEP src still needs this mapper's
        // shared decap terms; leave them in place.
        if (vtep_still_referenced(mapper)) {
            continue;
        }

        auto entries = mapper->get_child_objs(SAI_OBJECT_TYPE_TUNNEL_MAP_ENTRY);
        if (!entries) continue;

        for (auto& entry_pair : *entries) {
            auto entry = entry_pair.second;
            sai_object_id_t term_oid;
            sai_deserialize_object_id(entry->get_id(), term_oid);

            auto term_it = m_vxlan_decap_term_map.find(term_oid);
            if (term_it == m_vxlan_decap_term_map.end()) continue;

            // Free only the terms this tunnel's VTEP src owns; a map entry can
            // hold terms for several VTEP sources.
            auto& vec = term_it->second;
            for (auto it = vec.begin(); it != vec.end();) {
                if (sai_ip_address_equal(it->src_ip, deleted_vtep)) {
                    remove_vxlan_decap_term(*it);
                    it = vec.erase(it);
                } else {
                    ++it;
                }
            }
            if (vec.empty()) {
                m_vxlan_decap_term_map.erase(term_it);
            }
        }
    }

    return SAI_STATUS_SUCCESS;
}


sai_status_t
TunnelManager::handle_l2_vxlan_tunnel_map_entry(
    _In_ const std::string& serializedObjectId,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    SaiCachedObject map_entry_obj(m_switch_db, SAI_OBJECT_TYPE_TUNNEL_MAP_ENTRY,
                                  serializedObjectId, attr_count, attr_list);

    int32_t tunnel_map_type = -1;
    uint32_t vni = 0;
    uint16_t vlan_id = 0;
    sai_object_id_t tunnel_map_oid = SAI_NULL_OBJECT_ID;

    sai_attribute_t attr;

    attr.id = SAI_TUNNEL_MAP_ENTRY_ATTR_TUNNEL_MAP_TYPE;
    if (map_entry_obj.get_attr(attr) == SAI_STATUS_SUCCESS) {
        tunnel_map_type = attr.value.s32;
    }

    attr.id = SAI_TUNNEL_MAP_ENTRY_ATTR_VNI_ID_KEY;
    if (map_entry_obj.get_attr(attr) == SAI_STATUS_SUCCESS) {
        vni = attr.value.u32;
    }

    attr.id = SAI_TUNNEL_MAP_ENTRY_ATTR_VLAN_ID_VALUE;
    if (map_entry_obj.get_attr(attr) == SAI_STATUS_SUCCESS) {
        vlan_id = attr.value.u16;
    }

    attr.id = SAI_TUNNEL_MAP_ENTRY_ATTR_TUNNEL_MAP;
    if (map_entry_obj.get_attr(attr) == SAI_STATUS_SUCCESS) {
        tunnel_map_oid = attr.value.oid;
    }

    // L3 VNET decap (VNI -> Virtual Router): install decap for each VTEP,
    // including the primary Loopback0 VTEP when no encapsulation NH exists.
    if (tunnel_map_type == SAI_TUNNEL_MAP_TYPE_VNI_TO_VIRTUAL_ROUTER_ID) {
        return install_l3_vxlan_decap_terms(serializedObjectId, vni, tunnel_map_oid,
                                            &map_entry_obj);
    }

    if (tunnel_map_type != SAI_TUNNEL_MAP_TYPE_VNI_TO_VLAN_ID) {
        return SAI_STATUS_SUCCESS;
    }

    if (vni == 0 || vlan_id == 0 || tunnel_map_oid == SAI_NULL_OBJECT_ID) {
        return SAI_STATUS_SUCCESS;
    }

    const bool had_l2_tunnel = m_l2_tunnel_map.find(vni) != m_l2_tunnel_map.end();
    sai_object_id_t map_entry_oid;
    sai_deserialize_object_id(serializedObjectId, map_entry_oid);
    auto fail_l2_map_create = [&](sai_status_t status) {
        if (!had_l2_tunnel && m_l2_tunnel_map.find(vni) != m_l2_tunnel_map.end()) {
            sai_status_t rollback_status = remove_l2_vxlan_tunnel_for_vni(vni);
            if (rollback_status != SAI_STATUS_SUCCESS) {
                SWSS_LOG_ERROR("Failed to roll back newly-created L2 VXLAN VNI %u: "
                               "status=%d", vni, rollback_status);
            }
        }
        return status;
    };

    // Find tunnels that reference this mapper. EVPN P2P tunnels carry an
    // explicit ENCAP_DST_IP and retain the existing object-owned behavior;
    // static tunnels defer to their stored exact P2P termination entry.
    auto mapper_obj = m_switch_db->get_sai_object(SAI_OBJECT_TYPE_TUNNEL_MAP,
        sai_serialize_object_id(tunnel_map_oid));
    if (!mapper_obj) {
        SWSS_LOG_ERROR("Tunnel map %s not found in DB",
            sai_serialize_object_id(tunnel_map_oid).c_str());
        return SAI_STATUS_SUCCESS;
    }

    auto tunnels = mapper_obj->get_child_objs(SAI_OBJECT_TYPE_TUNNEL);
    if (!tunnels) {
        return SAI_STATUS_SUCCESS;
    }

    for (auto& tunnel_pair : *tunnels) {
        // A SAI object can have wrapper instances in more than one parent's
        // child map. Resolve it through SaiObjectDB so we get the canonical
        // TUNNEL parent wrapper that owns its termination-entry children.
        auto tunnel_obj = m_switch_db->get_sai_object(
            SAI_OBJECT_TYPE_TUNNEL, tunnel_pair.first);
        if (!tunnel_obj) {
            SWSS_LOG_ERROR("Unable to resolve canonical tunnel object %s",
                           tunnel_pair.first.c_str());
            continue;
        }

        attr.id = SAI_TUNNEL_ATTR_TYPE;
        if (tunnel_obj->get_attr(attr) != SAI_STATUS_SUCCESS ||
            attr.value.s32 != SAI_TUNNEL_TYPE_VXLAN) continue;

        attr.id = SAI_TUNNEL_ATTR_ENCAP_DST_IP;
        if (tunnel_obj->get_attr(attr) == SAI_STATUS_SUCCESS) {
            sai_ip_address_t dst_ip = attr.value.ipaddr;

            attr.id = SAI_TUNNEL_ATTR_ENCAP_SRC_IP;
            if (tunnel_obj->get_attr(attr) != SAI_STATUS_SUCCESS) {
                continue;
            }
            sai_ip_address_t src_ip = attr.value.ipaddr;

            uint32_t vni_sw_if_index;
            sai_status_t status = create_l2_vxlan_tunnel_for_vni(
                src_ip, dst_ip, vni, vlan_id, vni_sw_if_index,
                SAI_NULL_OBJECT_ID, map_entry_oid);
            if (status != SAI_STATUS_SUCCESS) {
                return fail_l2_map_create(status);
            }
            continue;
        }

        auto terms = tunnel_obj->get_child_objs(
            SAI_OBJECT_TYPE_TUNNEL_TERM_TABLE_ENTRY);
        if (terms && !terms->empty()) {
            for (auto& term_pair : *terms) {
                VxlanTermMatch match;
                bool applicable = false;
                sai_status_t status = get_exact_vxlan_term_match(
                    term_pair.second.get(), match, applicable);
                if (status != SAI_STATUS_SUCCESS) {
                    return fail_l2_map_create(status);
                }
                if (!applicable) {
                    continue;
                }

                sai_object_id_t term_oid;
                sai_deserialize_object_id(term_pair.first, term_oid);

                uint32_t vni_sw_if_index;
                status = create_l2_vxlan_tunnel_for_vni(
                    match.local_ip, match.remote_ip, vni, vlan_id,
                    vni_sw_if_index, term_oid, map_entry_oid);
                if (status != SAI_STATUS_SUCCESS) {
                    return fail_l2_map_create(status);
                }
            }
            continue;
        }

        SWSS_LOG_NOTICE("Late mapper entry for VNI %u is waiting for "
                        "tunnel %s termination entry",
                        vni, tunnel_obj->get_id().c_str());
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t
TunnelManager::remove_l2_vxlan_tunnel_for_vni(_In_ uint32_t vni)
{
    SWSS_LOG_ENTER();

    auto it = m_l2_tunnel_map.find(vni);
    if (it == m_l2_tunnel_map.end()) {
        return SAI_STATUS_SUCCESS;
    }

    TunnelVPPData& tunnel_data = it->second;

    if (set_sw_interface_l2_bridge_by_index(
            tunnel_data.sw_if_index, tunnel_data.vlan_id,
            false, VPP_API_PORT_TYPE_NORMAL) != 0) {
        SWSS_LOG_ERROR("Failed to detach L2 VXLAN tunnel VNI=%u sw_if=%u "
                       "from VLAN=%u", vni, tunnel_data.sw_if_index,
                       tunnel_data.vlan_id);
        return SAI_STATUS_FAILURE;
    }

    const uint32_t drop_table = tunnel_data.l2_output_drop_table_index;
    if (drop_table != ~0u &&
        vpp_classify_set_interface_l2_tables_by_index(
            tunnel_data.sw_if_index, ~0u, ~0u, ~0u,
            false /* output */) != 0) {
        SWSS_LOG_ERROR("Failed to unbind L2 VXLAN output guard for VNI=%u; "
                       "leaving the tunnel outside the bridge", vni);
        return SAI_STATUS_FAILURE;
    }

    vpp_vxlan_tunnel_t req;
    memset(&req, 0, sizeof(req));
    req.vni = tunnel_data.vni;
    req.src_port = m_vxlan_port;
    req.dst_port = m_vxlan_port;
    req.instance = ~0;
    req.decap_next_index = ~0;
    sai_ip_address_t_to_vpp_ip_addr_t(tunnel_data.src_ip, req.src_address);
    sai_ip_address_t_to_vpp_ip_addr_t(tunnel_data.dst_ip, req.dst_address);

    if (remove_vpp_vxlan_encap(req, tunnel_data, /*skip_neighbor=*/true) !=
        SAI_STATUS_SUCCESS) {
        if (drop_table != ~0u &&
            vpp_classify_set_interface_l2_tables_by_index(
                tunnel_data.sw_if_index, drop_table, drop_table, drop_table,
                false /* output */) != 0) {
            SWSS_LOG_ERROR("Failed to restore L2 VXLAN output guard for VNI=%u; "
                           "leaving the tunnel outside the bridge", vni);
            return SAI_STATUS_FAILURE;
        }
        if (set_sw_interface_l2_bridge_by_index(
                tunnel_data.sw_if_index, tunnel_data.vlan_id,
                true, VPP_API_PORT_TYPE_NORMAL) != 0) {
            SWSS_LOG_ERROR("Failed to restore L2 VXLAN tunnel VNI=%u to "
                           "VLAN=%u after delete failure", vni,
                           tunnel_data.vlan_id);
            return SAI_STATUS_FAILURE;
        }
        // Bridge reattachment enables learning, so restore the decap-only
        // setting just as on initial creation.
        if (tunnel_data.tunnel_term_oid != SAI_NULL_OBJECT_ID &&
            set_l2_interface_flags_by_index(tunnel_data.sw_if_index,
                                            VPP_BD_FLAG_LEARN, false) != 0) {
            SWSS_LOG_ERROR("Failed to disable learning on restored L2 VXLAN "
                           "tunnel VNI=%u sw_if=%u", vni,
                           tunnel_data.sw_if_index);
        }
        return SAI_STATUS_FAILURE;
    }

    if (drop_table != ~0u && vpp_classify_table_delete(drop_table) != 0) {
        SWSS_LOG_ERROR("Failed to delete L2 VXLAN output guard table %u for VNI=%u",
                       drop_table, vni);
    }

    SWSS_LOG_NOTICE("Removed L2 VXLAN tunnel VNI=%u (sw_if=%u, VLAN=%u)",
                    vni, tunnel_data.sw_if_index, tunnel_data.vlan_id);
    m_l2_tunnel_map.erase(it);

    return SAI_STATUS_SUCCESS;
}

sai_status_t
TunnelManager::remove_l2_vxlan_tunnel_term(
    _In_ const std::string& serializedObjectId)
{
    SWSS_LOG_ENTER();

    sai_object_id_t term_oid;
    sai_deserialize_object_id(serializedObjectId, term_oid);

    std::vector<uint32_t> owned_vnis;
    for (const auto& entry : m_l2_tunnel_map) {
        if (entry.second.tunnel_term_oid == term_oid) {
            owned_vnis.push_back(entry.first);
        }
    }

    for (uint32_t vni : owned_vnis) {
        sai_status_t status = remove_l2_vxlan_tunnel_for_vni(vni);
        if (status != SAI_STATUS_SUCCESS) {
            return status;
        }
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t
TunnelManager::handle_l2_vxlan_tunnel_map_entry_removal(
    _In_ const std::string& serializedObjectId)
{
    SWSS_LOG_ENTER();

    // L3 VNET decap teardown, keyed by map-entry OID so it works
    // even if the SAI object is already gone from the DB.
    {
        sai_object_id_t term_oid;
        sai_deserialize_object_id(serializedObjectId, term_oid);
        auto term_it = m_vxlan_decap_term_map.find(term_oid);
        if (term_it != m_vxlan_decap_term_map.end()) {
            for (auto& td : term_it->second) {
                remove_vxlan_decap_term(td);
            }
            m_vxlan_decap_term_map.erase(term_it);
            return SAI_STATUS_SUCCESS;
        }
    }

    sai_object_id_t map_entry_oid;
    sai_deserialize_object_id(serializedObjectId, map_entry_oid);
    auto entry_obj = m_switch_db->get_sai_object(
        SAI_OBJECT_TYPE_TUNNEL_MAP_ENTRY, serializedObjectId);
    if (!entry_obj) {
        return SAI_STATUS_SUCCESS;
    }

    // Only handle VNI_TO_VLAN_ID type
    sai_attribute_t attr;
    attr.id = SAI_TUNNEL_MAP_ENTRY_ATTR_TUNNEL_MAP_TYPE;
    if (entry_obj->get_attr(attr) != SAI_STATUS_SUCCESS ||
        attr.value.s32 != SAI_TUNNEL_MAP_TYPE_VNI_TO_VLAN_ID) {
        return SAI_STATUS_SUCCESS;
    }

    // Get VNI
    attr.id = SAI_TUNNEL_MAP_ENTRY_ATTR_VNI_ID_KEY;
    if (entry_obj->get_attr(attr) != SAI_STATUS_SUCCESS) {
        return SAI_STATUS_SUCCESS;
    }
    uint32_t vni = attr.value.u32;
    auto tunnel_it = m_l2_tunnel_map.find(vni);
    if (tunnel_it != m_l2_tunnel_map.end() &&
        tunnel_it->second.tunnel_term_oid != SAI_NULL_OBJECT_ID) {
        if (tunnel_it->second.tunnel_map_entry_oid != map_entry_oid) {
            return SAI_STATUS_SUCCESS;
        }
    }
    return remove_l2_vxlan_tunnel_for_vni(vni);
}

sai_status_t
TunnelManager::vxlan_secondary_vtep_local_receive(
                    _In_ const sai_ip_address_t& vtep_ip,
                    _In_ bool is_add)
{
    SWSS_LOG_ENTER();

    bool is_v6 = (vtep_ip.addr_family == SAI_IP_ADDR_FAMILY_IPV6);

    // Refcount key: address family byte + raw address bytes. Multiple decap
    // terms (distinct VNIs) can share one secondary VTEP IP; the VRF0 local-
    // receive must be programmed once (first ref) and torn down once (last
    // ref), otherwise removing one term breaks decap for its siblings.
    std::string key(1, is_v6 ? '6' : '4');
    if (is_v6) {
        key.append(reinterpret_cast<const char *>(vtep_ip.addr.ip6), 16);
    } else {
        key.append(reinterpret_cast<const char *>(&vtep_ip.addr.ip4), 4);
    }

    if (is_add) {
        auto it = m_vtep_local_receive_refcount.find(key);
        if (it != m_vtep_local_receive_refcount.end()) {
            it->second++;   // route already present; just take a reference
            return SAI_STATUS_SUCCESS;
        }
        // first user: fall through and program the route below
    } else {
        auto it = m_vtep_local_receive_refcount.find(key);
        if (it == m_vtep_local_receive_refcount.end()) {
            return SAI_STATUS_SUCCESS;   // nothing programmed for this VTEP
        }
        if (--it->second > 0) {
            return SAI_STATUS_SUCCESS;   // other terms still need the route
        }
        m_vtep_local_receive_refcount.erase(it);
        // last user: fall through and delete the route below
    }

    vpp_ip_route_t *route = (vpp_ip_route_t *)
        calloc(1, sizeof(vpp_ip_route_t) + sizeof(vpp_ip_nexthop_t));
    if (!route) {
        SWSS_LOG_ERROR("Failed to allocate memory for VTEP local-receive route");
        return SAI_STATUS_FAILURE;
    }

    route->vrf_id = 0;          // underlay / default VRF
    route->is_multipath = false;
    route->nexthop_cnt = 1;

    if (is_v6) {
        route->prefix_len = 128;
        route->prefix_addr.sa_family = AF_INET6;
        memcpy(route->prefix_addr.addr.ip6.sin6_addr.s6_addr, vtep_ip.addr.ip6,
               sizeof(route->prefix_addr.addr.ip6.sin6_addr.s6_addr));
        route->nexthop[0].addr.sa_family = AF_INET6;
    } else {
        route->prefix_len = 32;
        route->prefix_addr.sa_family = AF_INET;
        route->prefix_addr.addr.ip4.sin_addr.s_addr = vtep_ip.addr.ip4;
        route->nexthop[0].addr.sa_family = AF_INET;
    }
    route->nexthop[0].type = VPP_NEXTHOP_LOCAL;
    route->nexthop[0].sw_if_index = (uint32_t)~0;

    int ret = ip_route_add_del(route, is_add);
    free(route);

    if (ret != 0) {
        SWSS_LOG_ERROR("Failed to %s VRF0 local-receive for secondary VTEP (ret %d)",
                       is_add ? "add" : "remove", ret);
        // Adjust the refcount to match the actual programmed state so a retry
        // can re-attempt (add failure = no ref held; delete failure = keep it).
        if (is_add) {
            m_vtep_local_receive_refcount.erase(key);
        } else {
            m_vtep_local_receive_refcount[key] = 1;
        }
        return SAI_STATUS_FAILURE;
    }

    if (is_add) {
        m_vtep_local_receive_refcount[key] = 1;
    }
    return SAI_STATUS_SUCCESS;
}

/*
 * Fill the decap-only VXLAN tunnel's destination address.
 *
 * VPP derives a tunnel's address-family from its SOURCE and then REJECTS any
 * tunnel whose src/dst families differ (vxlan_add_del_tunnel_clean_input:
 * "ip46_address_is_ip4(src) != ip46_address_is_ip4(dst)"). For IPv4 the
 * decap-only sentinel dst is the unspecified 0.0.0.0. For IPv6 the unspecified
 * :: is misclassified as IPv4 by ip46_address_is_ip4() (its top 12 bytes are
 * zero), which would fail the family-match check and reject the tunnel. So for
 * IPv6 use a non-zero, non-multicast placeholder derived from src: it is
 * guaranteed IPv6-classified and different from src. The dst is never used for
 * forwarding - decap matches the source-independent (src, vni) wildcard that
 * patch 0017 registers on tunnel add.
 */
static void
vxlan_decap_term_set_dst(_In_ const vpp_ip_addr_t& src, _Out_ vpp_ip_addr_t& dst)
{
    SWSS_LOG_ENTER();

    if (src.sa_family == AF_INET6) {
        dst.sa_family = AF_INET6;
        memcpy(&dst.addr.ip6.sin6_addr, &src.addr.ip6.sin6_addr,
               sizeof(struct in6_addr));
        /* dst is a decap-only placeholder, never used for forwarding; it only
         * has to pass VPP's family-match check, i.e. ip46_address_is_ip4(dst)
         * must stay false (top 12 bytes not all zero) and dst != src. Flipping
         * s6_addr[8] gives dst != src, but on its own it is not airtight: for a
         * src whose top 12 bytes are zero except s6_addr[8] == 0x01 the flip
         * would zero all 12 bytes and misclassify dst as IPv4. Also force a
         * fixed non-zero, non-multicast high-order byte so the top 12 bytes can
         * never collapse to zero regardless of src (correct by construction). */
        dst.addr.ip6.sin6_addr.s6_addr[8] ^= 0x01;   /* guarantee dst != src */
        dst.addr.ip6.sin6_addr.s6_addr[0]  = 0x20;   /* non-zero, non-multicast */
    } else {
        dst.sa_family = AF_INET;
        dst.addr.ip4.sin_addr.s_addr = 0;   /* 0.0.0.0 sentinel */
    }
}

sai_status_t
TunnelManager::create_vxlan_decap_term(
                    _In_  const sai_ip_address_t& vtep_ip,
                    _In_  uint32_t vni,
                    _In_  std::shared_ptr<IpVrfInfo> ip_vrf,
                    _Out_ TunnelVPPData& tunnel_data)
{
    SWSS_LOG_ENTER();

    char            hw_bvi_ifname[32];
    auto            router_mac = get_router_mac();
    auto            bvi_mac = router_mac.data();
    vpp_ip_route_t  bvi_ip_prefix;

    // A local VTEP already has an interface receive route. A secondary VTEP
    // needs one installed below; both need this decap tunnel because a VNET
    // can have a decap mapper without any encapsulation next hop.
    bool is_local_vtep = false;
    refresh_interfaces_list();
    {
        vpp_ip_addr_t    probe_ip;
        sai_ip_address_t vtep_probe = vtep_ip;
        sai_ip_address_t_to_vpp_ip_addr_t(vtep_probe, probe_ip);
        uint32_t owner_if = 0;
        if (vpp_sw_interface_find_by_ip(&probe_ip, 0, &owner_if) == 0) {
            is_local_vtep = true;
        }
    }

    // Allocate a BD + BVI so the decapped inner packet is L3-routed by a BVI
    // bound to the decap-mapper VRF (mirrors create_vpp_vxlan_decap).
    int bd_id = m_switch_db->dynamic_bd_id_pool.alloc();
    if (bd_id == -1) {
        SWSS_LOG_ERROR("Failed to allocate bridge domain ID for vxlan decap term");
        return SAI_STATUS_FAILURE;
    }
    tunnel_data.bd_id = bd_id;
    tunnel_data.ip_vrf = ip_vrf;
    tunnel_data.vni = vni;
    tunnel_data.src_ip = vtep_ip;
    memset(&tunnel_data.dst_ip, 0, sizeof(tunnel_data.dst_ip));
    tunnel_data.dst_ip.addr_family = vtep_ip.addr_family;

    if (create_bvi_interface(bvi_mac, bd_id) != 0) {
        SWSS_LOG_ERROR("Failed to create bvi interface for vxlan decap term");
        m_switch_db->dynamic_bd_id_pool.free(bd_id);
        return SAI_STATUS_FAILURE;
    }
    refresh_interfaces_list();

    snprintf(hw_bvi_ifname, sizeof(hw_bvi_ifname), "bvi%u", bd_id);

    // Rollback the BD/BVI state allocated so far. Used on any setup failure
    // before the decap tunnel is created (mirrors the checked ladder below).
    auto rollback_bd_bvi = [&]() {
        delete_bvi_interface(hw_bvi_ifname);
        m_switch_db->dynamic_bd_id_pool.free(bd_id);
        refresh_interfaces_list();
        vpp_bridge_domain_add_del(bd_id, false);
    };

    if (interface_set_state(hw_bvi_ifname, true) != 0) {
        SWSS_LOG_ERROR("VXLAN decap term: failed to set BVI %s up for VNI %u",
                       hw_bvi_ifname, vni);
        rollback_bd_bvi();
        return SAI_STATUS_FAILURE;
    }
    if (set_sw_interface_l2_bridge(hw_bvi_ifname, bd_id, true, VPP_API_PORT_TYPE_BVI) != 0) {
        SWSS_LOG_ERROR("VXLAN decap term: failed to bridge BVI %s into bd %u for VNI %u",
                       hw_bvi_ifname, bd_id, vni);
        rollback_bd_bvi();
        return SAI_STATUS_FAILURE;
    }
    if (set_interface_vrf(hw_bvi_ifname, 0, ip_vrf->m_vrf_id, ip_vrf->m_is_ipv6) != 0) {
        SWSS_LOG_ERROR("VXLAN decap term: failed to set VRF %u on BVI %s for VNI %u",
                       ip_vrf->m_vrf_id, hw_bvi_ifname, vni);
        rollback_bd_bvi();
        return SAI_STATUS_FAILURE;
    }

    // Synthetic per-BD addresses L3-enable both families on the BVI so a v4
    // VTEP can still route inner v6 and vice-versa (mirrors decap path).
    uint16_t offset = (uint16_t)((uint16_t)(bd_id - SwitchVpp::dynamic_bd_id_base) + 2);

    bvi_ip_prefix.prefix_len = 32;
    bvi_ip_prefix.prefix_addr.sa_family = AF_INET;
    {
        struct sockaddr_in *sin = &bvi_ip_prefix.prefix_addr.addr.ip4;
        sin->sin_addr.s_addr = htonl(offset);
    }
    if (interface_ip_address_add_del(hw_bvi_ifname, &bvi_ip_prefix, true) != 0) {
        SWSS_LOG_ERROR("VXLAN decap term: failed to add v4 BVI addr on %s for VNI %u",
                       hw_bvi_ifname, vni);
        rollback_bd_bvi();
        return SAI_STATUS_FAILURE;
    }

    bvi_ip_prefix.prefix_len = 128;
    bvi_ip_prefix.prefix_addr.sa_family = AF_INET6;
    {
        struct sockaddr_in6 *sin6 = &bvi_ip_prefix.prefix_addr.addr.ip6;
        memset(&sin6->sin6_addr, 0, sizeof(struct in6_addr));
        sin6->sin6_addr.s6_addr[14] = (uint8_t)((offset >> 8) & 0xFF);
        sin6->sin6_addr.s6_addr[15] = (uint8_t)(offset & 0xFF);
    }
    if (interface_ip_address_add_del(hw_bvi_ifname, &bvi_ip_prefix, true) != 0) {
        SWSS_LOG_ERROR("VXLAN decap term: failed to add v6 BVI addr on %s for VNI %u",
                       hw_bvi_ifname, vni);
        rollback_bd_bvi();
        return SAI_STATUS_FAILURE;
    }

    // Create the decap-capable VXLAN tunnel: src = VTEP IP, dst = unspecified.
    // Its add registers the source-independent decap entry (VTEP IP, VNI).
    vpp_vxlan_tunnel_t req;
    memset(&req, 0, sizeof(req));
    req.dst_port = m_vxlan_port;
    req.src_port = m_vxlan_port;
    req.instance = ~0;
    req.vni = vni;
    req.decap_next_index = ~0;
    req.decap_any = true;
    sai_ip_address_t vtep_ip_nc = vtep_ip;
    sai_ip_address_t_to_vpp_ip_addr_t(vtep_ip_nc, req.src_address);
    vxlan_decap_term_set_dst(req.src_address, req.dst_address);

    if (create_vpp_vxlan_encap(req, tunnel_data, /*skip_neighbor=*/true) != SAI_STATUS_SUCCESS) {
        SWSS_LOG_ERROR("VXLAN decap term: failed to create tunnel for VNI %u", vni);
        rollback_bd_bvi();
        return SAI_STATUS_FAILURE;
    }

    // Bridge the tunnel into the BD so decapped frames reach the BVI.
    if (set_sw_interface_l2_bridge_by_index(tunnel_data.sw_if_index, bd_id, true,
                                            VPP_API_PORT_TYPE_NORMAL) != 0) {
        SWSS_LOG_ERROR("VXLAN decap term: failed to bridge tunnel for VNI %u", vni);
        remove_vpp_vxlan_encap(req, tunnel_data, /*skip_neighbor=*/true);
        rollback_bd_bvi();
        return SAI_STATUS_FAILURE;
    }

    // A secondary VTEP needs an explicit underlay local-receive route.
    if (!is_local_vtep &&
        vxlan_secondary_vtep_local_receive(vtep_ip, true) != SAI_STATUS_SUCCESS) {
        SWSS_LOG_ERROR("VXLAN decap term: failed to add VRF0 local-receive for VNI %u", vni);
        set_sw_interface_l2_bridge_by_index(tunnel_data.sw_if_index, bd_id, false,
                                            VPP_API_PORT_TYPE_NORMAL);
        remove_vpp_vxlan_encap(req, tunnel_data, /*skip_neighbor=*/true);
        rollback_bd_bvi();
        return SAI_STATUS_FAILURE;
    }
    tunnel_data.owns_local_receive = !is_local_vtep;

    SWSS_LOG_NOTICE("VXLAN decap term: installed VTEP decap VNI %u bd %u sw_if %u",
                    vni, bd_id, tunnel_data.sw_if_index);
    return SAI_STATUS_SUCCESS;
}

sai_status_t
TunnelManager::remove_vxlan_decap_term(_In_ TunnelVPPData& tunnel_data)
{
    SWSS_LOG_ENTER();

    char hw_bvi_ifname[32];

    if (tunnel_data.owns_local_receive) {
        vxlan_secondary_vtep_local_receive(tunnel_data.src_ip, false);
    }

    // Unbridge + delete the decap VXLAN tunnel.
    vpp_vxlan_tunnel_t req;
    memset(&req, 0, sizeof(req));
    req.dst_port = m_vxlan_port;
    req.src_port = m_vxlan_port;
    req.instance = ~0;
    req.vni = tunnel_data.vni;
    req.decap_next_index = ~0;
    sai_ip_address_t src_nc = tunnel_data.src_ip;
    sai_ip_address_t_to_vpp_ip_addr_t(src_nc, req.src_address);
    vxlan_decap_term_set_dst(req.src_address, req.dst_address);

    set_sw_interface_l2_bridge_by_index(tunnel_data.sw_if_index, tunnel_data.bd_id,
                                        false, VPP_API_PORT_TYPE_NORMAL);
    remove_vpp_vxlan_encap(req, tunnel_data, /*skip_neighbor=*/true);

    // Delete the BVI + bridge domain.
    snprintf(hw_bvi_ifname, sizeof(hw_bvi_ifname), "bvi%u", tunnel_data.bd_id);
    delete_bvi_interface(hw_bvi_ifname);
    m_switch_db->dynamic_bd_id_pool.free(tunnel_data.bd_id);
    refresh_interfaces_list();
    vpp_bridge_domain_add_del(tunnel_data.bd_id, false);

    SWSS_LOG_NOTICE("VXLAN decap term: removed VTEP decap VNI %u bd %u",
                    tunnel_data.vni, tunnel_data.bd_id);
    return SAI_STATUS_SUCCESS;
}

TunnelManagerIpIp::TunnelManagerIpIp(SwitchVpp *switch_db) : m_switch_db(switch_db) {}

uint8_t
TunnelManagerIpIp::map_sai_to_vpp_flags(const SaiObject* tunnel_obj)
{
    SWSS_LOG_ENTER();

    uint8_t vpp_flags = 0;
    sai_attribute_t attr;

    // Decap ECN mode
    attr.id = SAI_TUNNEL_ATTR_DECAP_ECN_MODE;
    if (tunnel_obj->get_attr(attr) == SAI_STATUS_SUCCESS) {
        if (attr.value.s32 == SAI_TUNNEL_DECAP_ECN_MODE_COPY_FROM_OUTER) {
            vpp_flags |= 0x10; // TUNNEL_API_ENCAP_DECAP_FLAG_DECAP_COPY_ECN
        }
    }

    // Decap TTL mode (uniform = copy from outer)
    attr.id = SAI_TUNNEL_ATTR_DECAP_TTL_MODE;
    if (tunnel_obj->get_attr(attr) == SAI_STATUS_SUCCESS) {
        if (attr.value.s32 == SAI_TUNNEL_TTL_MODE_UNIFORM_MODEL) {
            vpp_flags |= 0x40; // TUNNEL_API_ENCAP_DECAP_FLAG_ENCAP_COPY_HOP_LIMIT
        }
    }

    // Decap DSCP mode (uniform = copy from outer)
    attr.id = SAI_TUNNEL_ATTR_DECAP_DSCP_MODE;
    if (tunnel_obj->get_attr(attr) == SAI_STATUS_SUCCESS) {
        if (attr.value.s32 == SAI_TUNNEL_DSCP_MODE_UNIFORM_MODEL) {
            vpp_flags |= 0x04; // TUNNEL_API_ENCAP_DECAP_FLAG_ENCAP_COPY_DSCP
        }
    }

    // Encap TTL mode (uniform = copy from inner)
    attr.id = SAI_TUNNEL_ATTR_ENCAP_TTL_MODE;
    if (tunnel_obj->get_attr(attr) == SAI_STATUS_SUCCESS) {
        if (attr.value.s32 == SAI_TUNNEL_TTL_MODE_UNIFORM_MODEL) {
            vpp_flags |= 0x40; // TUNNEL_API_ENCAP_DECAP_FLAG_ENCAP_COPY_HOP_LIMIT
        }
    }

    // Encap DSCP mode (uniform = copy from inner)
    attr.id = SAI_TUNNEL_ATTR_ENCAP_DSCP_MODE;
    if (tunnel_obj->get_attr(attr) == SAI_STATUS_SUCCESS) {
        if (attr.value.s32 == SAI_TUNNEL_DSCP_MODE_UNIFORM_MODEL) {
            vpp_flags |= 0x04; // TUNNEL_API_ENCAP_DECAP_FLAG_ENCAP_COPY_DSCP
        }
    }

    // Encap ECN mode: STANDARD, the SAI default, copies the inner ECN (RFC 6040 4.1)
    attr.id = SAI_TUNNEL_ATTR_ENCAP_ECN_MODE;
    if (tunnel_obj->get_attr(attr) != SAI_STATUS_SUCCESS ||
        attr.value.s32 == SAI_TUNNEL_ENCAP_ECN_MODE_STANDARD) {
        vpp_flags |= 0x08; // TUNNEL_API_ENCAP_DECAP_FLAG_ENCAP_COPY_ECN
    }

    return vpp_flags;
}

uint32_t TunnelManagerIpIp::resolve_vrf_id(_In_ sai_object_id_t vr_oid)
{
    SWSS_LOG_ENTER();

    if (vr_oid == SAI_NULL_OBJECT_ID) {
        return 0;
    }
    auto vrf = m_switch_db->vpp_get_ip_vrf(vr_oid);
    if (vrf) {
        SWSS_LOG_INFO("IpIp: VR %s -> VRF %u",
                      sai_serialize_object_id(vr_oid).c_str(), vrf->m_vrf_id);
        return vrf->m_vrf_id;
    }
    SWSS_LOG_WARN("IpIp: vr %s not found, using default vrf",
                  sai_serialize_object_id(vr_oid).c_str());
    return 0;
}

uint32_t TunnelManagerIpIp::resolve_vrf_from_rif(_In_ sai_object_id_t rif_oid)
{
    SWSS_LOG_ENTER();

    auto rif_obj = m_switch_db->get_sai_object(SAI_OBJECT_TYPE_ROUTER_INTERFACE,
                                               sai_serialize_object_id(rif_oid));
    if (rif_obj) {
        sai_attribute_t rif_attr;
        rif_attr.id = SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID;
        if (rif_obj->get_attr(rif_attr) == SAI_STATUS_SUCCESS) {
            return resolve_vrf_id(rif_attr.value.oid);
        }
    }
    SWSS_LOG_WARN("IpIp: could not resolve vrf from rif %s",
                  sai_serialize_object_id(rif_oid).c_str());
    return 0;
}

/*
 * Shared helpers for IPIP tunnel create/remove.
 *
 * create_ipip_vpp_tunnel:
 *   1. vpp_ipip_tunnel_add
 *   2. refresh_interfaces_list + interface_set_state UP
 *   3. set_interface_vrf (overlay VRF assignment)
 *   4. vpp_sw_interface_find_by_ip + sw_interface_set_unnumbered
 *   Cleans up (deletes tunnel) on partial failure.
 *
 * remove_ipip_vpp_tunnel:
 *   1. interface_set_state DOWN
 *   2. vpp_ipip_tunnel_del
 *
 * Both use a refcount map keyed by {src, dst, mode} to avoid creating
 * duplicate VPP tunnels when both decap (tunnel_term) and encap (nexthop)
 * paths request the same tunnel (e.g. MuxTunnel0 P2P).
 */

bool TunnelManagerIpIp::IpIpTunnelKey::operator==(const IpIpTunnelKey &o) const
{
    SWSS_LOG_ENTER();

    return (mode == o.mode &&
            sai_ip_address_equal(src, o.src) &&
            sai_ip_address_equal(dst, o.dst));
}

std::size_t TunnelManagerIpIp::IpIpTunnelKeyHash::operator()(const IpIpTunnelKey &k) const
{
    SWSS_LOG_ENTER();

    std::size_t h = std::hash<uint8_t>()(k.mode);
    uint64_t tmp;
    if (k.src.addr_family == SAI_IP_ADDR_FAMILY_IPV4) {
        h ^= std::hash<uint32_t>()(k.src.addr.ip4) << 1;
    }
    else {
        memcpy(&tmp, k.src.addr.ip6, sizeof(tmp));
        h ^= std::hash<uint64_t>()(tmp) << 1;
    }
    if (k.dst.addr_family == SAI_IP_ADDR_FAMILY_IPV4) {
        h ^= std::hash<uint32_t>()(k.dst.addr.ip4) << 2;
    }
    else {
        memcpy(&tmp, k.dst.addr.ip6, sizeof(tmp));
        h ^= std::hash<uint64_t>()(tmp) << 2;
    }
    return h;
}

sai_status_t TunnelManagerIpIp::create_ipip_vpp_tunnel(
    _Inout_ vpp_ipip_tunnel_t &req,
    _In_ uint32_t vrf_id,
    _Out_ uint32_t &sw_if_index)
{
    SWSS_LOG_ENTER();

    sw_if_index = 0;

    // Check if a VPP tunnel with the same {src, dst, mode} already exists
    IpIpTunnelKey key;
    memset(&key, 0, sizeof(key));
    vpp_ip_addr_t_to_sai_ip_address_t(req.src_address, key.src);
    vpp_ip_addr_t_to_sai_ip_address_t(req.dst_address, key.dst);
    key.mode = req.mode;

    auto ref_it = m_ipip_tunnel_refcount.find(key);
    if (ref_it != m_ipip_tunnel_refcount.end()) {
        // Tunnel already exists
        ref_it->second.refcount++;
        sw_if_index = ref_it->second.sw_if_index;
        SWSS_LOG_NOTICE("IpIp: reusing existing vpp ipip tunnel sw_if=%u (refcount=%u)",
                        sw_if_index, ref_it->second.refcount);
        return SAI_STATUS_SUCCESS;
    }

    // Create the IPIP tunnel
    int ret = vpp_ipip_tunnel_add(&req, &sw_if_index);
    if (ret < 0) {
        SWSS_LOG_ERROR("IpIp: vpp_ipip_tunnel_add failed: ret=%d", ret);
        return SAI_STATUS_FAILURE;
    }

    // Set the tunnel interface up
    refresh_interfaces_list();
    const char *ifname = vpp_get_swif_name(sw_if_index);
    if (!ifname) {
        SWSS_LOG_ERROR("IpIp: could not get interface name for sw_if_index=%u", sw_if_index);
        return SAI_STATUS_FAILURE;
    }

    ret = interface_set_state(ifname, true);
    if (ret < 0) {
        SWSS_LOG_ERROR("IpIp: failed to set interface up for %s ret=%d", ifname, ret);
        return SAI_STATUS_FAILURE;
    }
    SWSS_LOG_NOTICE("IpIp: tunnel %s (sw_if=%u) set UP", ifname, sw_if_index);

    // Assign the tunnel interface to the overlay vrf
    bool is_ipv6 = (req.src_address.sa_family == AF_INET6);
    ret = set_interface_vrf(ifname, 0, vrf_id, is_ipv6);
    if (ret < 0) {
        SWSS_LOG_ERROR("IpIp: failed to set interface vrf for %s vrf=%u is_ipv6=%d ret=%d",
                       ifname, vrf_id, is_ipv6, ret);
        return SAI_STATUS_FAILURE;
    }

    // Set unnumbered — borrow IP from the interface that owns req.src_address
    uint32_t owner_sw_if_index = 0;
    if (vpp_sw_interface_find_by_ip(&req.src_address, vrf_id, &owner_sw_if_index) == 0) {
        ret = sw_interface_set_unnumbered(sw_if_index, owner_sw_if_index, true);
        if (ret < 0) {
            SWSS_LOG_ERROR("IpIp: failed to set interface unnumbered sw_if=%u use sw_if=%u ret=%d",
                           sw_if_index, owner_sw_if_index, ret);
            return SAI_STATUS_FAILURE;
        } else {
            const char *owner_ifname = vpp_get_swif_name(owner_sw_if_index);
            SWSS_LOG_NOTICE("IpIp: sw_if=%u set unnumbered using %s (sw_if=%u)",
                            sw_if_index, owner_ifname ? owner_ifname : "?", owner_sw_if_index);
        }
    }
    else {
        char ip_str[INET6_ADDRSTRLEN];
        vpp_ip_addr_t_to_string(&req.src_address, ip_str, sizeof(ip_str));
        SWSS_LOG_WARN("IpIp: no interface found for IP %s in vrf %u yet, deferring unnumbered for sw_if=%u",
                      ip_str, vrf_id, sw_if_index);
        PendingUnnumbered pending;
        pending.sw_if_index = sw_if_index;
        pending.src_address = req.src_address;
        pending.vrf_id = vrf_id;
        m_pending_unnumbered.emplace(std::string(ip_str), pending);
    }

    IpIpTunnelRef ref_data;
    ref_data.sw_if_index = sw_if_index;
    ref_data.refcount = 1;
    m_ipip_tunnel_refcount[key] = ref_data;

    return SAI_STATUS_SUCCESS;
}

sai_status_t TunnelManagerIpIp::remove_ipip_vpp_tunnel(_In_ uint32_t sw_if_index)
{
    SWSS_LOG_ENTER();

    auto it = std::find_if(m_ipip_tunnel_refcount.begin(), m_ipip_tunnel_refcount.end(),
                           [sw_if_index](const auto &entry)
                           { return entry.second.sw_if_index == sw_if_index; });
    if (it != m_ipip_tunnel_refcount.end()) {
        if (--it->second.refcount > 0) {
            SWSS_LOG_NOTICE("IpIp: tunnel sw_if=%u still in use (refcount=%u), skipping delete",
                            sw_if_index, it->second.refcount);
            return SAI_STATUS_SUCCESS;
        }
        m_ipip_tunnel_refcount.erase(it);
    }

    // Remove any pending unnumbered entries for this tunnel
    for (auto pending_it = m_pending_unnumbered.begin(); pending_it != m_pending_unnumbered.end(); ) {
        if (pending_it->second.sw_if_index == sw_if_index) {
            pending_it = m_pending_unnumbered.erase(pending_it);
        } else {
            ++pending_it;
        }
    }

    const char *ifname = vpp_get_swif_name(sw_if_index);
    if (ifname) {
        interface_set_state(ifname, false);
    }

    int ret = vpp_ipip_tunnel_del(sw_if_index);
    if (ret < 0) {
        SWSS_LOG_ERROR("IpIp: vpp_ipip_tunnel_del failed for sw_if=%u: ret=%d", sw_if_index, ret);
        return SAI_STATUS_FAILURE;
    }

    return SAI_STATUS_SUCCESS;
}

void TunnelManagerIpIp::retry_pending_unnumbered(_In_ const vpp_ip_addr_t &rif_ip)
{
    SWSS_LOG_ENTER();

    if (m_pending_unnumbered.empty()) {
        return;
    }

    vpp_ip_addr_t *rif_ip_ptr = const_cast<vpp_ip_addr_t *>(&rif_ip);

    char ip_str[INET6_ADDRSTRLEN];
    vpp_ip_addr_t_to_string(rif_ip_ptr, ip_str, sizeof(ip_str));

    auto range = m_pending_unnumbered.equal_range(std::string(ip_str));
    if (range.first == range.second) {
        return;
    }

    SWSS_LOG_NOTICE("IpIp: retrying %zu pending unnumbered for IP %s",
                    (size_t)std::distance(range.first, range.second), ip_str);

    // Find the owner interface for this IP
    uint32_t owner_sw_if_index = 0;
    if (vpp_sw_interface_find_by_ip(rif_ip_ptr, range.first->second.vrf_id, &owner_sw_if_index) != 0) {
        SWSS_LOG_WARN("IpIp: still no owner interface for IP %s", ip_str);
        return;
    }

    const char *owner_ifname = vpp_get_swif_name(owner_sw_if_index);
    for (auto it = range.first; it != range.second; ) {
        int ret = sw_interface_set_unnumbered(it->second.sw_if_index, owner_sw_if_index, true);
        if (ret < 0) {
            SWSS_LOG_ERROR("IpIp: deferred unnumbered failed sw_if=%u use sw_if=%u ret=%d",
                           it->second.sw_if_index, owner_sw_if_index, ret);
            ++it;
            continue;
        }
        SWSS_LOG_NOTICE("IpIp: deferred unnumbered succeeded sw_if=%u using %s (sw_if=%u)",
                        it->second.sw_if_index, owner_ifname ? owner_ifname : "?", owner_sw_if_index);
        it = m_pending_unnumbered.erase(it);
    }
}

sai_status_t TunnelManagerIpIp::create_ipip_tunnel_term(
    _In_ const std::string &serializedObjectId,
    _In_ sai_object_id_t switch_id,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_object_id_t term_oid;
    sai_deserialize_object_id(serializedObjectId, term_oid);

    // Parse attributes from the tunnel term table entry
    sai_ip_address_t dst_ip;   // our local IP (packet destination = tunnel src)
    sai_ip_address_t src_ip;   // remote peer IP (packet source = tunnel dst)
    sai_object_id_t tunnel_oid = SAI_NULL_OBJECT_ID;
    sai_object_id_t vr_oid = SAI_NULL_OBJECT_ID;
    int32_t term_type = SAI_TUNNEL_TERM_TABLE_ENTRY_TYPE_P2MP;
    bool has_src_ip = false;

    memset(&dst_ip, 0, sizeof(dst_ip));
    memset(&src_ip, 0, sizeof(src_ip));

    for (uint32_t i = 0; i < attr_count; i++) {
        switch (attr_list[i].id) {
            case SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_DST_IP:
                dst_ip = attr_list[i].value.ipaddr;
                break;
            case SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_SRC_IP:
                src_ip = attr_list[i].value.ipaddr;
                has_src_ip = true;
                break;
            case SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_ACTION_TUNNEL_ID:
                tunnel_oid = attr_list[i].value.oid;
                break;
            case SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_TYPE:
                term_type = attr_list[i].value.s32;
                break;
            case SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_VR_ID:
                vr_oid = attr_list[i].value.oid;
                break;
            default:
                break;
        }
    }

    if (tunnel_oid == SAI_NULL_OBJECT_ID) {
        SWSS_LOG_ERROR("IpIp: missing tunnel id in tunnel term %s", serializedObjectId.c_str());
        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    // Look up the referenced tunnel object from SaiObjectDB to read TTL/DSCP/ECN modes
    auto tunnel_db_obj = m_switch_db->get_sai_object(SAI_OBJECT_TYPE_TUNNEL,
                                                     sai_serialize_object_id(tunnel_oid));
    if (!tunnel_db_obj) {
        SWSS_LOG_ERROR("IpIp: tunnel object %s not found in object DB",
                       sai_serialize_object_id(tunnel_oid).c_str());
        return SAI_STATUS_ITEM_NOT_FOUND;
    }

    // Map sai_tunnel_term_table_entry_type_t -> VPP IPIP tunnel mode.
    // SAI P2MP term means "one local endpoint decapsulates traffic from many
    // remote peers" -- this is decap-only and maps to VPP's MP2P mode, which
    // (unlike MP / NBMA) does not require per-peer TEIB next-hops.
    uint8_t vpp_mode = IpIpTunnelVPPData::TUNNEL_API_MODE_MP2P;
    switch (term_type) {
        case SAI_TUNNEL_TERM_TABLE_ENTRY_TYPE_P2P:
            vpp_mode = IpIpTunnelVPPData::TUNNEL_API_MODE_P2P;
            break;
        case SAI_TUNNEL_TERM_TABLE_ENTRY_TYPE_P2MP:
            vpp_mode = IpIpTunnelVPPData::TUNNEL_API_MODE_MP2P;
            break;
        case SAI_TUNNEL_TERM_TABLE_ENTRY_TYPE_MP2P:
        case SAI_TUNNEL_TERM_TABLE_ENTRY_TYPE_MP2MP:
        default:
            SWSS_LOG_ERROR("IpIp Tunnel: Unsupported tunnel term type %d", term_type);
            return SAI_STATUS_NOT_SUPPORTED;
    }

    // Map SAI TTL/DSCP/ECN modes to VPP flags
    uint8_t vpp_flags = map_sai_to_vpp_flags(tunnel_db_obj.get());

    // Build VPP IPIP tunnel request
    // SAI term DST_IP = our local IP = VPP tunnel src
    // SAI term SRC_IP = remote peer = VPP tunnel dst (0.0.0.0 for P2MP)
    vpp_ipip_tunnel_t req;
    memset(&req, 0, sizeof(req));
    req.instance = ~0;
    req.mode = vpp_mode;
    req.flags = vpp_flags;
    // ENCAP_INNER_HASH: hash on inner 5-tuple for ECMP for P2P ipip tunnel
    if (vpp_mode == IpIpTunnelVPPData::TUNNEL_API_MODE_P2P) {
        req.flags |= 0x20;
    }

    sai_ip_address_t_to_vpp_ip_addr_t(dst_ip, req.src_address);
    if (has_src_ip && vpp_mode == IpIpTunnelVPPData::TUNNEL_API_MODE_P2P) {
        sai_ip_address_t_to_vpp_ip_addr_t(src_ip, req.dst_address);
    } else {
        // MP2P (P2MP-style decap): dst = 0.0.0.0 (already zeroed)
        req.dst_address.sa_family = dst_ip.addr_family == SAI_IP_ADDR_FAMILY_IPV4 ? AF_INET : AF_INET6;
    }

    // Resolve overlay VRF for the tunnel interface
    uint32_t overlay_vrf_id = resolve_vrf_id(vr_oid);

    // Create tunnel, assign VRF, bring up, set unnumbered
    uint32_t sw_if_index = 0;
    sai_status_t status = create_ipip_vpp_tunnel(req, overlay_vrf_id, sw_if_index);
    if (status != SAI_STATUS_SUCCESS) {
        SWSS_LOG_ERROR("IpIp: vpp ipip tunnel creation failed for term %s",
                       serializedObjectId.c_str());
        return status;
    }

    // Bookkeeping
    IpIpTunnelVPPData data;
    data.sw_if_index = sw_if_index;
    data.src_ip = dst_ip;        // local endpoint
    data.dst_ip = src_ip;        // remote endpoint
    data.mode = static_cast<IpIpTunnelVPPData::tunnel_mode>(vpp_mode);
    data.flags = vpp_flags;
    data.tunnel_oid = tunnel_oid;
    data.vrf_id = overlay_vrf_id;
    m_ipip_term_map[term_oid] = data;

    SWSS_LOG_NOTICE("IpIp: created tunnel term %s: sw_if=%u mode=%s",
                    serializedObjectId.c_str(), sw_if_index,
                    vpp_mode == IpIpTunnelVPPData::TUNNEL_API_MODE_P2P ? "P2P" : "MP2P");

    return SAI_STATUS_SUCCESS;
}

sai_status_t TunnelManagerIpIp::remove_ipip_tunnel_term(
    _In_ const std::string &serializedObjectId)
{
    SWSS_LOG_ENTER();

    sai_object_id_t term_oid;
    sai_deserialize_object_id(serializedObjectId, term_oid);

    auto it = m_ipip_term_map.find(term_oid);
    if (it == m_ipip_term_map.end()) {
        SWSS_LOG_WARN("IPIP: tunnel term %s not found in map, skipping",
                        serializedObjectId.c_str());
        return SAI_STATUS_SUCCESS;
    }

    uint32_t sw_if_index = it->second.sw_if_index;
    sai_status_t status = remove_ipip_vpp_tunnel(sw_if_index);
    if (status != SAI_STATUS_SUCCESS) {
        SWSS_LOG_ERROR("IpIp: remove_ipip_vpp_tunnel failed for term %s sw_if=%u",
                        serializedObjectId.c_str(), sw_if_index);
        return status;
    }

    SWSS_LOG_NOTICE("IpIp: removed IPIP tunnel term %s (sw_if=%u)",
                    serializedObjectId.c_str(), sw_if_index);

    m_ipip_term_map.erase(it);

    return SAI_STATUS_SUCCESS;
}

sai_status_t TunnelManagerIpIp::ipip_encap_nexthop_action(
    _In_ const SaiObject *tunnel_nh_obj,
    _In_ const SaiObject *tunnel_obj,
    _In_ Action action)
{
    SWSS_LOG_ENTER();

    sai_object_id_t nh_oid;
    sai_deserialize_object_id(tunnel_nh_obj->get_id(), nh_oid);

    sai_attribute_t attr;

    // Verify the tunnel type is IPINIP
    attr.id = SAI_TUNNEL_ATTR_TYPE;
    CHECK_STATUS_QUIET(tunnel_obj->get_mandatory_attr(attr));
    if (attr.value.s32 != SAI_TUNNEL_TYPE_IPINIP) {
        SWSS_LOG_ERROR("IpIp Encap: tunnel %s type %d is not IPINIP",
                        tunnel_obj->get_id().c_str(), attr.value.s32);
        return SAI_STATUS_NOT_SUPPORTED;
    }

    // Get ENCAP_SRC_IP from the tunnel object (our local endpoint)
    attr.id = SAI_TUNNEL_ATTR_ENCAP_SRC_IP;
    CHECK_STATUS_QUIET(tunnel_obj->get_mandatory_attr(attr));
    sai_ip_address_t src_ip = attr.value.ipaddr;

    // Get the destination IP from the nexthop object (remote peer)
    attr.id = SAI_NEXT_HOP_ATTR_IP;
    CHECK_STATUS_QUIET(tunnel_nh_obj->get_mandatory_attr(attr));
    sai_ip_address_t dst_ip = attr.value.ipaddr;

    // Get the tunnel overlay interface
    attr.id = SAI_TUNNEL_ATTR_OVERLAY_INTERFACE;
    CHECK_STATUS_QUIET(tunnel_obj->get_mandatory_attr(attr));
    sai_object_id_t tunnel_overlay_if_oid = attr.value.oid;

    if (action == Action::CREATE) {
        uint8_t vpp_flags = map_sai_to_vpp_flags(tunnel_obj);

        vpp_ipip_tunnel_t req;
        memset(&req, 0, sizeof(req));
        req.instance = ~0;
        req.mode = IpIpTunnelVPPData::TUNNEL_API_MODE_P2P;
        // ENCAP_INNER_HASH: hash on inner 5-tuple for ECMP for P2P ipip tunnel
        req.flags = vpp_flags | 0x20;

        sai_ip_address_t_to_vpp_ip_addr_t(src_ip, req.src_address);
        sai_ip_address_t_to_vpp_ip_addr_t(dst_ip, req.dst_address);

        uint32_t sw_if_index = 0;
        // Resolve VRF from the overlay RIF's virtual router
        uint32_t vrf_id = resolve_vrf_from_rif(tunnel_overlay_if_oid);
        sai_status_t status = create_ipip_vpp_tunnel(req, vrf_id, sw_if_index);
        if (status != SAI_STATUS_SUCCESS) {
            SWSS_LOG_ERROR("IpIp Encap: create_ipip_vpp_tunnel failed for nexthop %s",
                            tunnel_nh_obj->get_id().c_str());
            return status;
        }

        // Bookkeeping
        IpIpTunnelVPPData data;
        data.sw_if_index = sw_if_index;
        data.src_ip = src_ip;
        data.dst_ip = dst_ip;
        data.mode = IpIpTunnelVPPData::TUNNEL_API_MODE_P2P;
        data.flags = vpp_flags;
        data.vrf_id = vrf_id;

        sai_object_id_t tunnel_oid;
        sai_deserialize_object_id(tunnel_obj->get_id(), tunnel_oid);
        data.tunnel_oid = tunnel_oid;

        m_ipip_encap_nh_map[nh_oid] = data;

        char src_str[INET6_ADDRSTRLEN], dst_str[INET6_ADDRSTRLEN];
        vpp_ip_addr_t_to_string(&req.src_address, src_str, sizeof(src_str));
        vpp_ip_addr_t_to_string(&req.dst_address, dst_str, sizeof(dst_str));
        SWSS_LOG_NOTICE("IpIp Encap: created P2P tunnel nexthop %s: src=%s dst=%s sw_if=%u",
                        tunnel_nh_obj->get_id().c_str(), src_str, dst_str, sw_if_index);

    } else if (action == Action::DELETE) {
        auto it = m_ipip_encap_nh_map.find(nh_oid);
        if (it == m_ipip_encap_nh_map.end()) {
            SWSS_LOG_WARN("IpIp Encap: ipip tunnel encap nexthop %s not found, skipping",
                          tunnel_nh_obj->get_id().c_str());
            return SAI_STATUS_SUCCESS;
        }

        uint32_t sw_if_index = it->second.sw_if_index;
        sai_status_t status = remove_ipip_vpp_tunnel(sw_if_index);
        if (status != SAI_STATUS_SUCCESS) {
            SWSS_LOG_ERROR("IpIp Encap: remove_ipip_vpp_tunnel failed for nexthop %s sw_if=%u",
                            tunnel_nh_obj->get_id().c_str(), sw_if_index);
            return status;
        }

        SWSS_LOG_NOTICE("IpIp Encap: removed ipip tunnel encap nexthop %s (sw_if=%u)",
                        tunnel_nh_obj->get_id().c_str(), sw_if_index);

        m_ipip_encap_nh_map.erase(it);
    }

    return SAI_STATUS_SUCCESS;
}
