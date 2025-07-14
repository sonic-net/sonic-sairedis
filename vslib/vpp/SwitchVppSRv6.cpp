/*
 * Copyright (c) 2024 Cisco and/or its affiliates.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <list>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>

#include "swss/logger.h"
#include "swss/exec.h"
#include "swss/converter.h"
#include "vppxlate/SaiVppXlate.h"

#include "SwitchVpp.h"
#include "SwitchVppUtils.h"
#include "TunnelManager.h"
#include "meta/sai_serialize.h"
#include "meta/NotificationPortStateChange.h"
#include "BitResourcePool.h"
#include "SaiObjectDB.h"

using namespace saivs;

TunnelManagerSRv6::TunnelManagerSRv6(SwitchVpp* switch_db): m_switch_db(switch_db)
{
    SWSS_LOG_ENTER();

    return;
}


sai_status_t TunnelManagerSRv6::fill_next_hop(
        _In_ sai_object_id_t next_hop_oid,
        _Out_ vpp_ip_addr_t &nh_ip,
        _Out_ uint32_t &vlan_idx,
        _Out_ char (&if_name)[64])
{
    SWSS_LOG_ENTER();

    sai_status_t    status = SAI_STATUS_SUCCESS;
    sai_attribute_t attr;
    sai_object_id_t port_oid;
    std::string     hwif_name;

    auto nexthop_obj = m_switch_db->get_sai_object(SAI_OBJECT_TYPE_NEXT_HOP, sai_serialize_object_id(next_hop_oid));
    if (!nexthop_obj) {
        SWSS_LOG_ERROR("Failed to find SAI_OBJECT_TYPE_NEXT_HOP SaiObject: %s", sai_serialize_object_id(next_hop_oid).c_str());
        return SAI_STATUS_FAILURE;
    }

    attr.id = SAI_NEXT_HOP_ATTR_TYPE;
    CHECK_STATUS_W_MSG(nexthop_obj->get_attr(attr), "Could not get next hop type.");
    if(attr.value.s32 != SAI_NEXT_HOP_TYPE_IP) {
        SWSS_LOG_ERROR("Nexthop type %d not supported.", attr.value.s32);
        return SAI_STATUS_FAILURE;
    }

    attr.id = SAI_NEXT_HOP_ATTR_IP;
    CHECK_STATUS_W_MSG(nexthop_obj->get_attr(attr), "IP address missing in nexthop %s", sai_serialize_object_id(next_hop_oid).c_str());
    sai_ip_address_t_to_vpp_ip_addr_t(attr.value.ipaddr, nh_ip);

    auto rif_obj = nexthop_obj->get_linked_object(SAI_OBJECT_TYPE_ROUTER_INTERFACE, SAI_NEXT_HOP_ATTR_ROUTER_INTERFACE_ID);
    if(!rif_obj) {
        SWSS_LOG_ERROR("Failed to find RIF for nexthop %s", sai_serialize_object_id(next_hop_oid).c_str());
        return SAI_STATUS_FAILURE;
    }

    attr.id = SAI_ROUTER_INTERFACE_ATTR_PORT_ID;
    CHECK_STATUS_W_MSG(rif_obj->get_attr(attr), "Port ID is missing in RIF object.");
    port_oid = attr.value.oid;

    attr.id = SAI_ROUTER_INTERFACE_ATTR_OUTER_VLAN_ID;
    if(rif_obj->get_attr(attr) == SAI_STATUS_SUCCESS)
    {
        vlan_idx = attr.value.u16;
    } else {
        vlan_idx = 0;
    }

    if(!m_switch_db->vpp_get_hwif_name(port_oid, vlan_idx, hwif_name)) {
        SWSS_LOG_WARN("VPP hwif name not found for port %s", sai_serialize_object_id(port_oid).c_str());
        return SAI_STATUS_FAILURE;
    }
    strncpy(if_name, hwif_name.c_str(), sizeof(if_name) -1);

    return status;
}

sai_status_t TunnelManagerSRv6::fill_my_sid_entry(
        _In_ const SaiObject* my_sid_obj,
        _Out_ vpp_my_sid_entry_t &my_sid)
{
    SWSS_LOG_ENTER();

    sai_status_t    status = SAI_STATUS_SUCCESS;
    sai_attribute_t attr;
    sai_my_sid_entry_t my_sid_entry;

    sai_deserialize_my_sid_entry(my_sid_obj->get_id(), my_sid_entry);

    struct sockaddr_in6 *sin6 =  &my_sid.localsid.addr.ip6;
    my_sid.localsid.sa_family = AF_INET6;
    sin6->sin6_family = AF_INET6;
    memcpy(sin6->sin6_addr.s6_addr, my_sid_entry.sid, sizeof(sin6->sin6_addr.s6_addr));

    auto vrf = m_switch_db->vpp_get_ip_vrf(my_sid_entry.vr_id);
    if(vrf == nullptr) {
        my_sid.fib_table = 0;
    } else {
        my_sid.fib_table = vrf->m_vrf_id;
    }

    attr.id = SAI_MY_SID_ENTRY_ATTR_ENDPOINT_BEHAVIOR;
    CHECK_STATUS_W_MSG(my_sid_obj->get_attr(attr), "Could not get behavior.");
    my_sid.behavior = (uint32_t) attr.value.u32;

    attr.id = SAI_MY_SID_ENTRY_ATTR_ENDPOINT_BEHAVIOR_FLAVOR;
    if(my_sid_obj->get_attr(attr) != SAI_STATUS_SUCCESS) {
        attr.value.u32 = SAI_MY_SID_ENTRY_ENDPOINT_BEHAVIOR_FLAVOR_NONE;
    }
    if(attr.value.u32 == SAI_MY_SID_ENTRY_ENDPOINT_BEHAVIOR_FLAVOR_PSP ||
       attr.value.u32 == SAI_MY_SID_ENTRY_ENDPOINT_BEHAVIOR_FLAVOR_PSP_AND_USP ||
       attr.value.u32 == SAI_MY_SID_ENTRY_ENDPOINT_BEHAVIOR_FLAVOR_PSP_AND_USD ||
       attr.value.u32 == SAI_MY_SID_ENTRY_ENDPOINT_BEHAVIOR_FLAVOR_PSP_AND_USP_AND_USD) {
        my_sid.end_psp = true;
    } else {
        my_sid.end_psp = false;
    }

    attr.id = SAI_MY_SID_ENTRY_ATTR_NEXT_HOP_ID;
    if(my_sid_obj->get_attr(attr) == SAI_STATUS_SUCCESS) {
        status = fill_next_hop(attr.value.oid, my_sid.nh_addr,
                           my_sid.vlan_index, my_sid.hwif_name);
    }

    return status;
}

sai_status_t TunnelManagerSRv6::add_remove_my_sid_entry(
        _In_ const SaiObject* my_sid_obj,
        _In_ bool is_del)
{
    SWSS_LOG_ENTER();

    sai_status_t          status = SAI_STATUS_SUCCESS;
    vpp_my_sid_entry_t    my_sid;
    memset(&my_sid, 0, sizeof(my_sid));

    status = fill_my_sid_entry(my_sid_obj, my_sid);
    if(status != SAI_STATUS_SUCCESS) {
        SWSS_LOG_ERROR("Failed to fill fields for my_sid entry %s, status %d",
                        my_sid_obj->get_id().c_str(), status);
        return SAI_STATUS_FAILURE;
    }

    status = vpp_my_sid_entry_add_del(&my_sid, is_del);
    if(status != SAI_STATUS_SUCCESS) {
        SWSS_LOG_ERROR("Failed to add/remove my_sid entry %s, status %d",
                        my_sid_obj->get_id().c_str(), status);
        status = SAI_STATUS_FAILURE;
    }

    return status;
}

sai_status_t TunnelManagerSRv6::create_my_sid_entry(
        _In_ const std::string &serializedObjectId,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_status_t status = SAI_STATUS_SUCCESS;

    SaiCachedObject my_sid_obj(m_switch_db, SAI_OBJECT_TYPE_MY_SID_ENTRY, serializedObjectId, attr_count, attr_list);
    status = add_remove_my_sid_entry(&my_sid_obj, false);

    if(status == SAI_STATUS_SUCCESS) {
        CHECK_STATUS(m_switch_db->create_internal(SAI_OBJECT_TYPE_MY_SID_ENTRY, serializedObjectId, switch_id, attr_count, attr_list));
    }

    SWSS_LOG_NOTICE("Create my_sid status: %d", status);

    return status;
}

sai_status_t TunnelManagerSRv6::remove_my_sid_entry(
        _In_ const std::string &serializedObjectId)
{
    SWSS_LOG_ENTER();

    sai_status_t status = SAI_STATUS_SUCCESS;

    SaiDBObject my_sid_obj(m_switch_db, SAI_OBJECT_TYPE_MY_SID_ENTRY, serializedObjectId);
    status = add_remove_my_sid_entry(&my_sid_obj, true);

    if(status == SAI_STATUS_SUCCESS) {
        CHECK_STATUS(m_switch_db->remove_internal(SAI_OBJECT_TYPE_MY_SID_ENTRY, serializedObjectId));
    }

    SWSS_LOG_NOTICE("Remove my_sid status: %d", status);

    return status;
}

static sai_status_t fill_segment_list(
        _In_ const sai_segment_list_t &sai_list,
        _Out_ vpp_sids_t &sidlist)
{
    SWSS_LOG_ENTER();

    sai_status_t status = SAI_STATUS_SUCCESS;
    if(sai_list.count > 16) {
        SWSS_LOG_ERROR("VPP max sid list size is 16, received %u", sai_list.count);
        return SAI_STATUS_FAILURE;
    }
    sidlist.num_sids = static_cast<uint8_t>(sai_list.count);
    for(uint32_t i = 0; i<sai_list.count; i++) {
        sidlist.sids[i].sa_family = AF_INET6;
        sidlist.sids[i].addr.ip6.sin6_family = AF_INET6;
        memcpy(sidlist.sids[i].addr.ip6.sin6_addr.s6_addr, sai_list.list[i], sizeof(uint8_t[16]));
    }
    return status;
}

vpp_ip_addr_t TunnelManagerSRv6::generate_bsid(
        _In_ sai_object_id_t sid_list_oid)
{
    SWSS_LOG_ENTER();

    vpp_ip_addr_t bsid;
    memset(&bsid, 0, sizeof(bsid));
    bsid.sa_family = AF_INET6;
    bsid.addr.ip6.sin6_family = AF_INET6;

    // Set first bytes of BSID to fixed value
    bsid.addr.ip6.sin6_addr.s6_addr[0] = 0xFE;
    bsid.addr.ip6.sin6_addr.s6_addr[1] = 0xC0;
    // Fill bottom half of BSID with SID list OID
    for(uint8_t i=0; i<8; i++) {
        bsid.addr.ip6.sin6_addr.s6_addr[15-i] = (uint8_t)(sid_list_oid>>(8*i)) & 0xFF;
    }

    return bsid;
}

sai_status_t TunnelManagerSRv6::fill_sidlist(
        _In_ const std::string &sidlist_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list,
        _Out_ vpp_sidlist_t &sidlist)
{
    SWSS_LOG_ENTER();

    sai_status_t    status = SAI_STATUS_SUCCESS;
    sai_object_id_t object_id;
    sai_deserialize_object_id(sidlist_id, object_id);

    generate_bsid(object_id);
    sidlist.bsid = generate_bsid(object_id);
    sidlist.weight = (uint32_t) ~0;         // Use default weight
    sidlist.type = 0;                       // Use SR_API_POLICY_TYPE_DEFAULT
    sidlist.fib_table = (uint32_t) ~0;      // Use default fib table
    sidlist.encap_src.sa_family = AF_INET6; // Use default encap source, leave address as 0's.

    for(uint32_t i = 0; i<attr_count; i++) {
        switch(attr_list[i].id) {
            case SAI_SRV6_SIDLIST_ATTR_SEGMENT_LIST:
                status = fill_segment_list(attr_list[i].value.segmentlist, sidlist.sids);
                break;
            case SAI_SRV6_SIDLIST_ATTR_TYPE:
                sidlist.is_encap = false;
                if(attr_list[i].value.u32 == SAI_SRV6_SIDLIST_TYPE_ENCAPS  ||
                   attr_list[i].value.u32 == SAI_SRV6_SIDLIST_TYPE_ENCAPS_RED) {
                    sidlist.is_encap = true;
                }
                break;
            default:
                SWSS_LOG_WARN("Unsupported attribute: %u", attr_list[i].id);
                break;
        }

        if(status != SAI_STATUS_SUCCESS) {
            break;
        }
    }
    return status;
}

sai_status_t TunnelManagerSRv6::create_sidlist_internal(
        _In_ const std::string &serializedObjectId,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_status_t          status = SAI_STATUS_SUCCESS;
    vpp_sidlist_t         sidlist;
    memset(&sidlist, 0, sizeof(sidlist));

    status = fill_sidlist(serializedObjectId, attr_count, attr_list, sidlist);
    if(status != SAI_STATUS_SUCCESS) {
        SWSS_LOG_ERROR("Failed to fill fields for sidlist %s, status %d",
                        serializedObjectId.c_str(), status);
        status = SAI_STATUS_FAILURE;
    }

    status = vpp_sidlist_add(&sidlist);
    if(status != SAI_STATUS_SUCCESS) {
        SWSS_LOG_ERROR("Failed to add sidlist %s, status %d",
                        serializedObjectId.c_str(), status);
        status = SAI_STATUS_FAILURE;
    }

    return status;
}

sai_status_t TunnelManagerSRv6::create_sidlist(
        _In_ const std::string &serializedObjectId,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_status_t status = SAI_STATUS_SUCCESS;

    status = create_sidlist_internal(serializedObjectId, attr_count, attr_list);

    if(status == SAI_STATUS_SUCCESS) {
        CHECK_STATUS(m_switch_db->create_internal(SAI_OBJECT_TYPE_SRV6_SIDLIST, serializedObjectId, switch_id, attr_count, attr_list));
    }

    SWSS_LOG_NOTICE("Create sidlist status: %d", status);

    return status;
}

sai_status_t TunnelManagerSRv6::remove_sidlist_internal(
        _In_ const std::string &serializedObjectId)
{
    SWSS_LOG_ENTER();

    sai_status_t status = SAI_STATUS_SUCCESS;
    sai_object_id_t object_id;

    sai_deserialize_object_id(serializedObjectId, object_id);

    auto bsid = generate_bsid(object_id);
    status = vpp_sidlist_del(&bsid);
    if(status != SAI_STATUS_SUCCESS) {
        SWSS_LOG_ERROR("Failed to delete sidlist %s, status %d",
                        serializedObjectId.c_str(), status);
        return SAI_STATUS_FAILURE;
    }

    return status;
}

sai_status_t TunnelManagerSRv6::remove_sidlist(
        _In_ const std::string &serializedObjectId)
{
    SWSS_LOG_ENTER();

    sai_status_t status = SAI_STATUS_SUCCESS;

    status = remove_sidlist_internal(serializedObjectId);

    if(status == SAI_STATUS_SUCCESS) {
       CHECK_STATUS(m_switch_db->remove_internal(SAI_OBJECT_TYPE_SRV6_SIDLIST, serializedObjectId))
    }

    SWSS_LOG_NOTICE("Remove sidlist status: %d", status);

    return status;
}

sai_status_t TunnelManagerSRv6::fill_bsid_set_src_addr(
        _In_ sai_object_id_t next_hop_oid,
        _Out_ vpp_ip_addr_t &bsid)
{
    SWSS_LOG_ENTER();

    sai_status_t status = SAI_STATUS_SUCCESS;
    sai_attribute_t attr;
    sai_object_id_t sr_list_oid;

    auto nexthop_obj = m_switch_db->get_sai_object(SAI_OBJECT_TYPE_NEXT_HOP, sai_serialize_object_id(next_hop_oid));
    if (!nexthop_obj) {
        SWSS_LOG_ERROR("Failed to find SAI_OBJECT_TYPE_NEXT_HOP SaiObject: %s", sai_serialize_object_id(next_hop_oid).c_str());
        return SAI_STATUS_FAILURE;
    }

    attr.id = SAI_NEXT_HOP_ATTR_SRV6_SIDLIST_ID;
    CHECK_STATUS_W_MSG(nexthop_obj->get_attr(attr), "SRV6_SIDLIST missing in nexthop %s", sai_serialize_object_id(next_hop_oid).c_str());
    sr_list_oid = attr.value.oid;

    auto tunnel_obj = nexthop_obj->get_linked_object(SAI_OBJECT_TYPE_TUNNEL, SAI_NEXT_HOP_ATTR_TUNNEL_ID);
    if(!tunnel_obj) {
        SWSS_LOG_ERROR("TUNNEL_ID missing in nexthop %s", sai_serialize_object_id(next_hop_oid).c_str());
        return SAI_STATUS_FAILURE;
    }

    attr.id = SAI_TUNNEL_ATTR_ENCAP_SRC_IP;
    if(tunnel_obj->get_attr(attr) == SAI_STATUS_SUCCESS)
    {
        vpp_ip_addr_t encap_src;
        sai_ip_address_t_to_vpp_ip_addr_t(attr.value.ipaddr, encap_src);
        // Per sr-steer encap address not supported, encap address will be applied to all.
        status = vpp_sr_set_encap_source(&encap_src);
        if(status != SAI_STATUS_SUCCESS) {
            SWSS_LOG_ERROR("Could not set encap source.");
            return SAI_STATUS_FAILURE;
        }
    }

    bsid = generate_bsid(sr_list_oid);

    return status;
}

static uint8_t get_prefix_length(
        _In_ const sai_ip_prefix_t &addr)
{
    SWSS_LOG_ENTER();

    uint8_t prefix_len = 0;
    if(addr.addr_family == SAI_IP_ADDR_FAMILY_IPV4) {
        prefix_len = static_cast<uint8_t>(__builtin_popcount(addr.mask.ip4));
    } else if (addr.addr_family == SAI_IP_ADDR_FAMILY_IPV6) {
        for(uint8_t i = 0; i<16; i++) {
            prefix_len += static_cast<uint8_t>(__builtin_popcount(addr.mask.ip6[i]));
        }
    } else {
        SWSS_LOG_ERROR("Unsupported address family %d", addr.addr_family);
        return ~0;
    }
    return prefix_len;
}

sai_status_t TunnelManagerSRv6::fill_sr_steer(
        _In_ const std::string &serializedObjectId,
        _In_ sai_object_id_t next_hop_oid,
        _Out_ vpp_sr_steer_t  &sr_steer)
{
    SWSS_LOG_ENTER();

    sai_status_t status = SAI_STATUS_SUCCESS;
    sai_route_entry_t route_entry;
    sai_deserialize_route_entry(serializedObjectId, route_entry);

    sai_ip_address_t sai_ip_addr = {route_entry.destination.addr_family, route_entry.destination.addr};
    sai_ip_address_t_to_vpp_ip_addr_t(sai_ip_addr, sr_steer.prefix.address);
    sr_steer.prefix.prefix_len = get_prefix_length(route_entry.destination);
    if(sr_steer.prefix.prefix_len == ~0) {
        SWSS_LOG_ERROR("Failed to get prefix length from mask");
        return SAI_STATUS_FAILURE;
    }

    auto vrf = m_switch_db->vpp_get_ip_vrf(route_entry.vr_id);
    if(vrf == nullptr) {
        sr_steer.fib_table = 0;
    } else {
        sr_steer.fib_table = vrf->m_vrf_id;
    }

    status = fill_bsid_set_src_addr(next_hop_oid, sr_steer.bsid);

    return status;
}

sai_status_t TunnelManagerSRv6::sr_steer_add_remove(
        _In_ const std::string &serializedObjectId,
        _In_ sai_object_id_t next_hop_oid,
        _In_ bool is_del)
{
    SWSS_LOG_ENTER();

    sai_status_t status = SAI_STATUS_SUCCESS;
    vpp_sr_steer_t sr_steer;
    memset(&sr_steer, 0, sizeof(sr_steer));

    status = fill_sr_steer(serializedObjectId, next_hop_oid, sr_steer);
    if(status != SAI_STATUS_SUCCESS) {
        SWSS_LOG_ERROR("Failed to fill fields for sr steer %s, status %d",
                        serializedObjectId.c_str(), status);
        return SAI_STATUS_FAILURE;
    }

    status = vpp_sr_steer_add_del(&sr_steer, is_del);
    if(status != SAI_STATUS_SUCCESS) {
        SWSS_LOG_ERROR("Failed to add/remove nexthop sidlist %s, status %d",
                        serializedObjectId.c_str(), status);
        status = SAI_STATUS_FAILURE;
    }

    return status;
}

sai_status_t TunnelManagerSRv6::create_sidlist_route_entry(
        _In_ const std::string &serializedObjectId,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_status_t status = SAI_STATUS_SUCCESS;
    sai_object_id_t nh_oid = SAI_NULL_OBJECT_ID;

    for(uint32_t i = 0; i<attr_count; i++) {
        if(attr_list[i].id == SAI_ROUTE_ENTRY_ATTR_NEXT_HOP_ID) {
            nh_oid = attr_list[i].value.oid;
            break;
        }
    }

    if(nh_oid == SAI_NULL_OBJECT_ID) {
        SWSS_LOG_ERROR("Next hop id missing in route entry %s", serializedObjectId.c_str());
        return SAI_STATUS_FAILURE;
    }

    status = sr_steer_add_remove(serializedObjectId, nh_oid, false);
    SWSS_LOG_NOTICE("Create route entry sidlist status: %d", status);
    return status;
}

sai_status_t TunnelManagerSRv6::remove_sidlist_route_entry(
        _In_ const std::string &serializedObjectId,
        _In_ sai_object_id_t next_hop_oid)
{
    SWSS_LOG_ENTER();

    sai_status_t status = SAI_STATUS_SUCCESS;

    status = sr_steer_add_remove(serializedObjectId, next_hop_oid, true);
    SWSS_LOG_NOTICE("Remove route entry sidlist status: %d", status);

    return status;
}

