#include "SwitchVpp.h"
#include "SwitchVppUtils.h"

#include "meta/sai_serialize.h"
#include "swss/logger.h"
#include "vppxlate/SaiVppXlate.h"

#include <cstdlib>
#include <cstring>

using namespace saivs;

sai_status_t SwitchVpp::createMirrorSession(
        _In_ sai_object_id_t object_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    const sai_attribute_value_t *value;
    uint32_t attr_index;
    MirrorSessionInfo info{};

    if(m_mirror_session_count >= m_maxMirrorSessions) {
        SWSS_LOG_ERROR("Cannot create mirror session %s: max mirror sessions reached (%d)", sai_serialize_object_id(object_id).c_str(), m_maxMirrorSessions);
        return SAI_STATUS_INSUFFICIENT_RESOURCES;
    }

    auto sid = sai_serialize_object_id(object_id);

    CHECK_STATUS(find_attrib_in_list(attr_count, attr_list, SAI_MIRROR_SESSION_ATTR_TYPE, &value, &attr_index));
    int32_t mirror_type = value->s32;

    if(mirror_type == SAI_MIRROR_SESSION_TYPE_LOCAL) {
        CHECK_STATUS(find_attrib_in_list(attr_count, attr_list, SAI_MIRROR_SESSION_ATTR_MONITOR_PORT, &value, &attr_index));
        sai_object_id_t monitor_port = value->oid;

        std::string hwif_name = m_ifaceRegistry.resolveHwIfName(monitor_port, 0);
        if(hwif_name.empty()) {
            SWSS_LOG_ERROR("Failed to get hwif name for monitor port %s", sai_serialize_object_id(monitor_port).c_str());
            return SAI_STATUS_FAILURE;
        }

        int sw_idx = get_sw_if_idx(hwif_name.c_str());
        if(sw_idx < 0) {
            SWSS_LOG_ERROR("Failed to get sw_if_index for hwif %s", hwif_name.c_str());
            return SAI_STATUS_FAILURE;
        }

        SWSS_LOG_INFO("SPAN mirror session info: monitor_port=%s, hwif_name=%s, sw_if_index=%d",
            sai_serialize_object_id(monitor_port).c_str(), hwif_name.c_str(), sw_idx);

        info.sw_if_index = (uint32_t)sw_idx;
        info.is_erspan = false;
    } else if(mirror_type == SAI_MIRROR_SESSION_TYPE_ENHANCED_REMOTE) {
        CHECK_STATUS(find_attrib_in_list(attr_count, attr_list, SAI_MIRROR_SESSION_ATTR_SRC_IP_ADDRESS, &value, &attr_index));
        sai_ip_address_t src_ip = value->ipaddr;

        CHECK_STATUS(find_attrib_in_list(attr_count, attr_list, SAI_MIRROR_SESSION_ATTR_DST_IP_ADDRESS, &value, &attr_index));
        sai_ip_address_t dst_ip = value->ipaddr;

        // SAI marks this MANDATORY_ON_CREATE but orchagent may program it via a
        // later SET, so default rather than fail and orphan every ACL rule.
        uint16_t gre_protocol = 0x88BE;
        if (find_attrib_in_list(attr_count, attr_list, SAI_MIRROR_SESSION_ATTR_GRE_PROTOCOL_TYPE, &value, &attr_index) == SAI_STATUS_SUCCESS) {
            gre_protocol = value->u16;
        }

        // Outer tunnel header TTL. Optional (SAI default 255).
        uint8_t session_ttl = 255;
        if (find_attrib_in_list(attr_count, attr_list, SAI_MIRROR_SESSION_ATTR_TTL, &value, &attr_index) == SAI_STATUS_SUCCESS) {
            session_ttl = value->u8;
        }

        int session_id = m_erspan_session_id_pool.alloc();
        if (session_id < 0) {
            SWSS_LOG_ERROR("ERSPAN session id pool exhausted (max 1024 in-flight sessions)");
            return SAI_STATUS_INSUFFICIENT_RESOURCES;
        }

        vpp_gre_tunnel_t tunnel{};
        sai_ip_address_t_to_vpp_ip_addr_t(src_ip, tunnel.src);
        sai_ip_address_t_to_vpp_ip_addr_t(dst_ip, tunnel.dst);
        // ERSPAN (type 2) is the only VPP GRE type whose L2 delivery path forwards
        // the ACL-injected clone. A non-zero gre_protocol tells the patched GRE
        // plugin to emit plain GRE (flags=0, no type-II shim), which is what SONiC
        // Everflow actually expects.
        tunnel.type = 2;
        tunnel.session_id = (uint16_t)session_id;
        tunnel.gre_protocol = gre_protocol;
        // The tunnel's underlay IP rewrite decrements the outer TTL once, so add
        // one to land on the session-configured value on the wire.
        if (session_ttl > 0 && session_ttl < 255) {
            tunnel.ttl = (uint8_t)(session_ttl + 1);
        } else {
            tunnel.ttl = session_ttl;
        }

        // Must stay monotonic and independent of the recyclable session id: naming
        // a new tunnel greN while VPP still tears down the old greN corrupts its
        // interface-name hash.
        tunnel.instance = m_next_gre_instance++;
        tunnel.outer_table_id = 0;

        uint32_t gre_instance = tunnel.instance;
        uint32_t gre_sw_if_index = 0;

        // Pin the encap to the one monitor port MirrorOrch already resolved, so the
        // constant outer header does not hash across the mirror-dst ECMP group.
        // Must run BEFORE the tunnel is created: the outer L2 midchain stacks at
        // creation time and does not reliably re-stack onto a later /32.
        info.src_ip = tunnel.src;
        info.dst_ip = tunnel.dst;
        info.monitor_pinned = false;
        info.monitor_port = SAI_NULL_OBJECT_ID;
        const sai_attribute_value_t *mon_value;
        uint32_t mon_index;
        const sai_attribute_value_t *mac_value;
        uint32_t mac_index;
        if (find_attrib_in_list(attr_count, attr_list, SAI_MIRROR_SESSION_ATTR_MONITOR_PORT, &mon_value, &mon_index) == SAI_STATUS_SUCCESS
            && find_attrib_in_list(attr_count, attr_list, SAI_MIRROR_SESSION_ATTR_DST_MAC_ADDRESS, &mac_value, &mac_index) == SAI_STATUS_SUCCESS)
        {
            applyErspanMonitor(info, mon_value->oid, mac_value->mac);
        }

        SWSS_LOG_NOTICE("Creating GRE mirror tunnel: type=%u session_id=%d instance=%u gre_protocol=0x%04x ttl=%u(session_ttl=%u)",
            tunnel.type, session_id, tunnel.instance, gre_protocol, tunnel.ttl, session_ttl);
        int ret = vpp_gre_tunnel_add_del(&tunnel, true, &gre_sw_if_index);
        if(ret != 0) {
            SWSS_LOG_ERROR("Failed to add GRE tunnel for ERSPAN session, ret=%d", ret);
            if(info.monitor_pinned) {
                pinErspanMonitor(info, false);
            }
            m_erspan_session_id_pool.free((uint32_t)session_id);
            return SAI_STATUS_FAILURE;
        }
        SWSS_LOG_NOTICE("GRE mirror tunnel created: session_id=%d sw_if_index=%u", session_id, gre_sw_if_index);

        // Set state by index rather than by name: the name lookup would need
        // refresh_interfaces_list(), whose repeated rebuild of VPP's interface-name
        // hashes churns the clib heap until a realloc trips its integrity check.
        std::string gre_ifname = "gre" + std::to_string(gre_instance);
        SWSS_LOG_INFO("gre tunnel created with ifname %s sw_if_index %u", gre_ifname.c_str(), gre_sw_if_index);
        int up_ret = interface_set_state_by_index(gre_sw_if_index, true);
        if(up_ret != 0) {
            SWSS_LOG_ERROR("Failed to bring up gre tunnel %s (sw_if_index %u), ret=%d", gre_ifname.c_str(), gre_sw_if_index, up_ret);
            vpp_gre_tunnel_add_del(&tunnel, false, &gre_sw_if_index);
            if(info.monitor_pinned) {
                pinErspanMonitor(info, false);
            }
            m_erspan_session_id_pool.free((uint32_t)session_id);
            return SAI_STATUS_FAILURE;
        }

        info.sw_if_index = gre_sw_if_index;
        info.is_erspan = true;
        info.session_id = (uint16_t)session_id;
        info.gre_instance = gre_instance;
    } else {
        SWSS_LOG_ERROR("Unsupported mirror session type %d", mirror_type);
        return SAI_STATUS_FAILURE;
    }

    sai_status_t create_status = create_internal(SAI_OBJECT_TYPE_MIRROR_SESSION, sid, switch_id, attr_count, attr_list);
    if(create_status != SAI_STATUS_SUCCESS) {
        SWSS_LOG_ERROR("Failed to create mirror session %s in SAI DB, status=%d", sid.c_str(), create_status);
        // Tunnel-scoped locals are out of scope here, so rebuild the delete key
        // from info exactly as removeMirrorSession does.
        if(info.is_erspan) {
            if(info.monitor_pinned) {
                pinErspanMonitor(info, false);
            }
            vpp_gre_tunnel_t tunnel{};
            tunnel.instance = info.gre_instance;
            tunnel.type = 2;
            tunnel.src = info.src_ip;
            tunnel.dst = info.dst_ip;
            tunnel.session_id = info.session_id;
            tunnel.outer_table_id = 0;
            uint32_t sw_if_index = 0;
            int ret = vpp_gre_tunnel_add_del(&tunnel, false, &sw_if_index);
            if(ret != 0) {
                SWSS_LOG_ERROR("Failed to remove GRE tunnel for ERSPAN session after SAI DB create failure, ret=%d", ret);
            }
            m_erspan_session_id_pool.free((uint32_t)info.session_id);
        }
        return create_status;
    }

    m_mirror_sessions[object_id] = info;
    m_mirror_session_count++;

    SWSS_LOG_NOTICE("Created mirror session %s, type=%d, sw_if_index=%u, is_erspan=%d, mirror session count: %u",
        sid.c_str(), mirror_type, info.sw_if_index, info.is_erspan, m_mirror_session_count);

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::removeMirrorSession(
        _In_ sai_object_id_t object_id)
{
    SWSS_LOG_ENTER();

    if(m_mirror_session_count == 0) {
        SWSS_LOG_ERROR("No mirror sessions to remove (m_mirror_session_count is 0)");
        return SAI_STATUS_FAILURE;
    }

    auto it = m_mirror_sessions.find(object_id);
    if(it == m_mirror_sessions.end()) {
        SWSS_LOG_ERROR("Mirror session %s not found", sai_serialize_object_id(object_id).c_str());
        return SAI_STATUS_ITEM_NOT_FOUND;
    }

    MirrorSessionInfo &info = it->second;

    if(info.is_erspan) {
        // Drop the pin before tearing down the tunnel that stacks on it.
        if(info.monitor_pinned) {
            pinErspanMonitor(info, false);
        }

        vpp_gre_tunnel_t tunnel{};
        // VPP keys the delete on (src, dst, fib, type, session_id), not instance.
        tunnel.instance = info.gre_instance;
        tunnel.type = 2;
        tunnel.src = info.src_ip;
        tunnel.dst = info.dst_ip;
        tunnel.session_id = info.session_id;
        tunnel.outer_table_id = 0;

        uint32_t sw_if_index = 0;
        int ret = vpp_gre_tunnel_add_del(&tunnel, false, &sw_if_index);
        if(ret != 0) {
            // Not usefully retryable, so drop the entry anyway rather than leak.
            SWSS_LOG_ERROR("Failed to remove GRE tunnel for ERSPAN session, ret=%d; freeing session id and dropping entry anyway", ret);
        }

        // After the delete attempt, so a re-alloc cannot collide with a live tunnel.
        m_erspan_session_id_pool.free((uint32_t)info.session_id);
    }

    // Unprogram and erase any port mirror bindings that still reference this
    // session, so no dangling binding is left pointing at a removed session.
    for(auto pmb_it = m_port_mirror_bindings.begin(); pmb_it != m_port_mirror_bindings.end(); ) {
        if(pmb_it->second.session_oid != object_id) {
            ++pmb_it;
            continue;
        }

        sai_object_id_t portId = pmb_it->first;
        auto port_sid = sai_serialize_object_id(portId);

        std::string src_hwif = m_ifaceRegistry.resolveHwIfName(portId, 0);
        if(src_hwif.empty()) {
            SWSS_LOG_WARN("Failed to get hwif name for port %s while removing mirror session %s; skipping VPP SPAN unprogramming",
                port_sid.c_str(), sai_serialize_object_id(object_id).c_str());
        } else {
            uint32_t src_sw_if = get_sw_if_idx(src_hwif.c_str());
            if(src_sw_if == (uint32_t)-1) {
                SWSS_LOG_WARN("Failed to get sw_if_index for hwif %s while removing mirror session %s; skipping VPP SPAN unprogramming",
                    src_hwif.c_str(), sai_serialize_object_id(object_id).c_str());
            } else {
                SWSS_LOG_INFO("VPP span disable: src_sw_if=%u, src_hwif_name=%s", src_sw_if, src_hwif.c_str());
                int ret = vpp_span_enable_disable(src_sw_if, ~0, 0, false);
                if(ret != 0){
                    SWSS_LOG_ERROR("vpp span disable failed for port %s: src_sw_if=%u, ret=%d", port_sid.c_str(), src_sw_if, ret);
                }
            }
        }

        pmb_it = m_port_mirror_bindings.erase(pmb_it);
    }

    CHECK_STATUS(remove_internal(SAI_OBJECT_TYPE_MIRROR_SESSION, sai_serialize_object_id(object_id)));

    m_mirror_sessions.erase(it);
    m_mirror_session_count--;

    SWSS_LOG_NOTICE("Removed mirror session %s, mirror session count: %u", sai_serialize_object_id(object_id).c_str(), m_mirror_session_count);

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::pinErspanMonitor(
        _In_ MirrorSessionInfo &info,
        _In_ bool is_add)
{
    SWSS_LOG_ENTER();

    if (info.monitor_hwif.empty())
    {
        return SAI_STATUS_SUCCESS;
    }

    init_vpp_client();

    // Route via the connected nexthop IP, not a static neighbor for the mirror dst
    // itself: the latter has no connected cover and would resolve to a drop.
    vpp_ip_route_t *route = (vpp_ip_route_t *)
        calloc(1, sizeof(vpp_ip_route_t) + sizeof(vpp_ip_nexthop_t));
    if (!route)
    {
        SWSS_LOG_ERROR("ERSPAN monitor pin: failed to allocate route");
        return SAI_STATUS_FAILURE;
    }

    route->prefix_addr = info.dst_ip;
    route->prefix_len = (info.dst_ip.sa_family == AF_INET) ? 32 : 128;
    route->vrf_id = 0;
    route->is_multipath = false;
    route->nexthop_cnt = 1;

    sai_ip_address_t_to_vpp_ip_addr_t(info.monitor_nh, route->nexthop[0].addr);
    route->nexthop[0].sw_if_index = (uint32_t)~0;
    route->nexthop[0].hwif_name = info.monitor_hwif.c_str();
    route->nexthop[0].weight = 1;
    route->nexthop[0].preference = 0;
    route->nexthop[0].type = VPP_NEXTHOP_NORMAL;
    route->nexthop[0].flags = 0;

    int ret = ip_route_add_del(route, is_add);
    free(route);

    if (ret != 0)
    {
        SWSS_LOG_ERROR("ERSPAN monitor pin %s failed, ret=%d", (is_add ? "add" : "del"), ret);
        return SAI_STATUS_FAILURE;
    }

    return SAI_STATUS_SUCCESS;
}

bool SwitchVpp::resolveMonitorNexthop(
        _In_ sai_object_id_t monitor_port,
        _In_ const sai_mac_t mac,
        _Out_ sai_ip_address_t &nexthop)
{
    SWSS_LOG_ENTER();

    auto pit = m_port_neighbor_mac.find(monitor_port);
    if (pit == m_port_neighbor_mac.end())
    {
        return false;
    }

    std::string macStr = sai_serialize_mac(mac);

    // Find a neighbor on the monitor port whose L2 address matches the mirror
    // DST_MAC; that neighbor's IP is the ERSPAN nexthop orchagent resolved.
    for (const auto &kv : pit->second)
    {
        if (kv.second == macStr)
        {
            sai_deserialize_ip_address(kv.first, nexthop);
            return true;
        }
    }

    return false;
}

sai_status_t SwitchVpp::applyErspanMonitor(
        _In_ MirrorSessionInfo &info,
        _In_ sai_object_id_t monitor_port,
        _In_ const sai_mac_t mac)
{
    SWSS_LOG_ENTER();

    // Tear down any existing pin before (re)computing the target.
    if (info.monitor_pinned)
    {
        pinErspanMonitor(info, false);
        info.monitor_pinned = false;
    }

    std::string hwif;
    sai_ip_address_t nh;

    bool resolved =
        monitor_port != SAI_NULL_OBJECT_ID
        && !(hwif = m_ifaceRegistry.resolveHwIfName(monitor_port, 0)).empty()
        && resolveMonitorNexthop(monitor_port, mac, nh);

    if (!resolved && monitor_port != SAI_NULL_OBJECT_ID)
    {
        // LAG uplink: orchagent's MONITOR_PORT is a physical member, but the
        // nexthop neighbor is learned on the parent PortChannel; resolve and
        // pin via the bond (BondEthernet<id>) instead of the member port.
        sai_object_id_t lag_oid = SAI_NULL_OBJECT_ID;
        if (getLagFromPort(monitor_port, lag_oid)
            && !(hwif = m_ifaceRegistry.resolveHwIfName(lag_oid, 0)).empty()
            && resolveMonitorNexthop(lag_oid, mac, nh))
        {
            resolved = true;
        }
    }

    if (resolved)
    {
        info.monitor_port = monitor_port;
        info.monitor_hwif = hwif;
        memcpy(info.monitor_mac, mac, sizeof(sai_mac_t));
        info.monitor_nh = nh;
        info.monitor_pinned = true;
        pinErspanMonitor(info, true);
        SWSS_LOG_NOTICE("ERSPAN monitor pinned: nexthop %s out %s",
            sai_serialize_ip_address(nh).c_str(), hwif.c_str());
    }
    else
    {
        // Monitor port or nexthop unresolved: record what we know for a later
        // re-point, but leave outer forwarding to the FIB (ECMP) for now.
        info.monitor_port = monitor_port;
        memcpy(info.monitor_mac, mac, sizeof(sai_mac_t));
        info.monitor_hwif.clear();
        info.monitor_pinned = false;
        SWSS_LOG_NOTICE("ERSPAN monitor port/nexthop unresolved; "
            "falling back to FIB-resolved forwarding for the mirror destination");
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::setMirrorSession(
        _In_ sai_object_id_t object_id,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    auto sid = sai_serialize_object_id(object_id);

    auto it = m_mirror_sessions.find(object_id);

    // Only ERSPAN sessions carry a monitor-port pin; everything else just persists.
    if (attr != nullptr && it != m_mirror_sessions.end() && it->second.is_erspan)
    {
        MirrorSessionInfo &info = it->second;

        if (attr->id == SAI_MIRROR_SESSION_ATTR_MONITOR_PORT)
        {
            // orchagent re-resolved the next hop; re-point using the current DST_MAC.
            applyErspanMonitor(info, attr->value.oid, info.monitor_mac);
            SWSS_LOG_NOTICE("ERSPAN session %s monitor port updated", sid.c_str());
        }
        else if (attr->id == SAI_MIRROR_SESSION_ATTR_DST_MAC_ADDRESS)
        {
            // Neighbor MAC changed; re-resolve against the current monitor port.
            applyErspanMonitor(info, info.monitor_port, attr->value.mac);
            SWSS_LOG_NOTICE("ERSPAN session %s dst mac updated", sid.c_str());
        }
    }

    return set_internal(SAI_OBJECT_TYPE_MIRROR_SESSION, sid, attr);
}

sai_status_t SwitchVpp::bindMirrorPort(
    _In_ sai_object_id_t portId,
    _In_ const sai_attribute_t* attr)
{
    SWSS_LOG_ENTER();

    auto sid = sai_serialize_object_id(portId);

    std::string src_hwif = m_ifaceRegistry.resolveHwIfName(portId, 0);
    if(src_hwif.empty()) {
        SWSS_LOG_ERROR("Failed to get hwif name for port %s", sid.c_str());
        return SAI_STATUS_FAILURE;
    } else {
        uint32_t src_sw_if = get_sw_if_idx(src_hwif.c_str());
        if(src_sw_if == (uint32_t)-1) {
            SWSS_LOG_ERROR("Failed to get sw_if_index for hwif %s", src_hwif.c_str());
            return SAI_STATUS_FAILURE;
        }

        if(attr->value.objlist.count > 0) {
            if(attr->value.objlist.count > 1) {
                SWSS_LOG_WARN("Multiple destination interfaces given (not supported); only the first will be used");
            }

            // bind
            sai_object_id_t session_oid = attr->value.objlist.list[0];
            auto it = m_mirror_sessions.find(session_oid);
            if(it == m_mirror_sessions.end()) {
                SWSS_LOG_ERROR("Mirror session %s not found for port %s", sai_serialize_object_id(session_oid).c_str(), sid.c_str());
                return SAI_STATUS_FAILURE;
            } else {
                auto pmb_it = m_port_mirror_bindings.find(portId);
                if(pmb_it == m_port_mirror_bindings.end()) {
                    PortMirrorBinding new_pmb{};
                    new_pmb.session_oid = session_oid;
                    new_pmb.rx = false;
                    new_pmb.tx = false;
                    new_pmb.dst_sw_if_idx = it->second.sw_if_index;
                    pmb_it = m_port_mirror_bindings.emplace(portId, new_pmb).first;
                }
                PortMirrorBinding& pmb = pmb_it->second;

                if(pmb.dst_sw_if_idx != it->second.sw_if_index) {
                    SWSS_LOG_ERROR("Mirror session dst_sw_if_index mismatch for port %s: pmb=%u, ms=%u", sid.c_str(), pmb.dst_sw_if_idx, it->second.sw_if_index);
                    return SAI_STATUS_FAILURE;
                }

                (attr->id == SAI_PORT_ATTR_INGRESS_MIRROR_SESSION) ? pmb.rx = true : pmb.tx = true;

                // 1 = RX, 2 = TX, 3 = both
                uint32_t state = (pmb.rx ? 1 : 0) | (pmb.tx ? 2 : 0);

                SWSS_LOG_INFO("Port mirror binding info for port %s: session_oid=%s, rx=%d, tx=%d, dst_sw_if_idx=%u", sid.c_str(), sai_serialize_object_id(session_oid).c_str(), pmb.rx, pmb.tx, pmb.dst_sw_if_idx);
                SWSS_LOG_INFO("VPP span enable: src_sw_if=%u, src_hwif_name=%s, dst_sw_if=%u, state=%u", src_sw_if, src_hwif.c_str(), it->second.sw_if_index, state);
                int ret = vpp_span_enable_disable(src_sw_if, it->second.sw_if_index, state, false);
                if(ret != 0){
                    SWSS_LOG_ERROR("vpp span enable disable failed for port %s: src_sw_if=%u, dst_sw_if=%u, state=%u, ret=%d", sid.c_str(), src_sw_if, it->second.sw_if_index, state, ret);
                    return SAI_STATUS_FAILURE;
                }
            }
        } else {
            auto pmb_it = m_port_mirror_bindings.find(portId);
            if(pmb_it == m_port_mirror_bindings.end()) {
                SWSS_LOG_WARN("No existing mirror session binding found for port %s; skipping VPP SPAN unprogramming", sid.c_str());
                return SAI_STATUS_SUCCESS;
            }

            PortMirrorBinding& pmb = pmb_it->second;

            (attr->id == SAI_PORT_ATTR_INGRESS_MIRROR_SESSION) ? pmb.rx = false : pmb.tx = false;

            // 1 = RX, 2 = TX, 3 = both, 0 = none
            uint32_t state = (pmb.rx ? 1 : 0) | (pmb.tx ? 2 : 0);

            if(state != 0) {
                // Other direction still active: reprogram with the remaining state.
                SWSS_LOG_INFO("VPP span update: src_sw_if=%u, src_hwif_name=%s, dst_sw_if=%u, state=%u", src_sw_if, src_hwif.c_str(), pmb.dst_sw_if_idx, state);
                int ret = vpp_span_enable_disable(src_sw_if, pmb.dst_sw_if_idx, state, false);
                if(ret != 0){
                    SWSS_LOG_ERROR("vpp span update failed for port %s: src_sw_if=%u, dst_sw_if=%u, state=%u, ret=%d", sid.c_str(), src_sw_if, pmb.dst_sw_if_idx, state, ret);
                    return SAI_STATUS_FAILURE;
                }
            } else {
                // Both directions now off: fully disable SPAN and drop the binding.
                m_port_mirror_bindings.erase(pmb_it);

                SWSS_LOG_INFO("VPP span disable: src_sw_if=%u, src_hwif_name=%s", src_sw_if, src_hwif.c_str());
                int ret = vpp_span_enable_disable(src_sw_if, ~0, 0, false);
                if(ret != 0){
                    SWSS_LOG_ERROR("vpp span disable failed for port %s: src_sw_if=%u, ret=%d", sid.c_str(), src_sw_if, ret);
                    return SAI_STATUS_FAILURE;
                }
            }
        }
    }
    return SAI_STATUS_SUCCESS;
}
