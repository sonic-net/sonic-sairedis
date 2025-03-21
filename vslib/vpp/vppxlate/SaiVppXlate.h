/*
 * Copyright (c) 2023 Cisco and/or its affiliates.
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

#ifndef __SAI_VPP_XLATE_H_
#define __SAI_VPP_XLATE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <netinet/in.h>

    typedef enum {
	VPP_NEXTHOP_NORMAL = 1,
	VPP_NEXTHOP_LOCAL = 2
    } vpp_nexthop_type_e;

    typedef struct vpp_ip_addr_ {
	int sa_family;
	union {
	    struct sockaddr_in ip4;
	    struct sockaddr_in6 ip6;
	} addr;
    } vpp_ip_addr_t;

    typedef struct vpp_ip_nexthop_ {
	    vpp_ip_addr_t addr;
        uint32_t      sw_if_index;
        const char *hwif_name;
	uint8_t weight;
        uint8_t preference;
	vpp_nexthop_type_e type;
        uint32_t flags;
    } vpp_ip_nexthop_t;

    typedef struct vpp_ip_route_ {
	vpp_ip_addr_t prefix_addr;
	unsigned int prefix_len;
        uint32_t vrf_id;
        bool is_multipath;
        unsigned int nexthop_cnt;
        vpp_ip_nexthop_t nexthop[0];
    } vpp_ip_route_t;

    typedef enum  {
        VPP_ACL_ACTION_API_DENY = 0,
        VPP_ACL_ACTION_API_PERMIT = 1,
        VPP_ACL_ACTION_API_PERMIT_STFULL = 2,
    } vpp_acl_action_e;

    typedef struct  _vpp_acl_rule {
        vpp_acl_action_e action;
        vpp_ip_addr_t src_prefix;
        vpp_ip_addr_t src_prefix_mask;
        vpp_ip_addr_t dst_prefix;
        vpp_ip_addr_t dst_prefix_mask;
        int proto;
        uint16_t srcport_or_icmptype_first;
        uint16_t srcport_or_icmptype_last;
        uint16_t dstport_or_icmpcode_first;
        uint16_t dstport_or_icmpcode_last;
        uint8_t tcp_flags_mask;
        uint8_t tcp_flags_value;
    } vpp_acl_rule_t;

    typedef struct _vpp_acl_ {
        char *acl_name;
        uint32_t count;
        vpp_acl_rule_t rules[0];
    } vpp_acl_t;

    typedef struct {
        vpp_ip_addr_t dst_prefix;
        vpp_ip_addr_t dst_prefix_mask;
        char hwif_name[64];
        uint8_t  ip_protocol;
        vpp_ip_addr_t next_hop_ip;
    } vpp_tunterm_acl_rule_t;

    typedef struct _vpp_tunterm_acl_ {
        char *acl_name;
        uint32_t count;
        vpp_tunterm_acl_rule_t rules[0];
    } vpp_tunterm_acl_t;


    typedef enum {
        VPP_IP_API_FLOW_HASH_SRC_IP = 1,
        VPP_IP_API_FLOW_HASH_DST_IP = 2,
        VPP_IP_API_FLOW_HASH_SRC_PORT = 4,
        VPP_IP_API_FLOW_HASH_DST_PORT = 8,
        VPP_IP_API_FLOW_HASH_PROTO = 16,
        VPP_IP_API_FLOW_HASH_REVERSE = 32,
        VPP_IP_API_FLOW_HASH_SYMETRIC = 64,
        VPP_IP_API_FLOW_HASH_FLOW_LABEL = 128,
    } vpp_ip_flow_hash_mask_e;

    typedef enum {
        VPP_API_BFD_STATE_ADMIN_DOWN = 0,
        VPP_API_BFD_STATE_DOWN = 1,
        VPP_API_BFD_STATE_INIT = 2,
        VPP_API_BFD_STATE_UP = 3,
    } vpp_api_bfd_state_e;

    typedef struct vpp_intf_status_ {
	char hwif_name[64];
	bool link_up;
    } vpp_intf_status_t;

    typedef struct vpp_bfd_state_notif_ {
        bool                multihop;
        uint32_t            sw_if_index;
        vpp_ip_addr_t       local_addr;
        vpp_ip_addr_t       peer_addr;
        vpp_api_bfd_state_e state;
    } vpp_bfd_state_notif_t;

    typedef enum {
	VPP_INTF_LINK_STATUS = 1,
        VPP_BFD_STATE_CHANGE,
    } vpp_event_type_e;

    typedef union vpp_event_data_ {
       vpp_intf_status_t     intf_status;
       vpp_bfd_state_notif_t bfd_notif;
    } vpp_event_data_t;

    typedef struct vpp_my_sid_entry_ {
        vpp_ip_addr_t localsid;
        bool end_psp;
        uint32_t behavior;
        char hwif_name[64];
        uint32_t vlan_index;
        uint32_t fib_table;
        vpp_ip_addr_t nh_addr;
    } vpp_my_sid_entry_t;

    typedef struct vpp_sid_list_ {
        uint8_t num_sids;
        vpp_ip_addr_t sids[16];
    } vpp_sids_t;

    typedef struct vpp_sidlist_ {
        vpp_ip_addr_t bsid;
        uint32_t weight;
        bool is_encap;
        uint8_t type;
        uint32_t fib_table;
        vpp_sids_t sids;
        vpp_ip_addr_t encap_src;
    } vpp_sidlist_t;

    typedef struct vpp_prefix_ {
        vpp_ip_addr_t address;
        uint8_t prefix_len;
    } vpp_prefix_t;

    typedef struct vpp_sr_steer_ {
        bool is_del;
        vpp_ip_addr_t bsid;
        uint32_t fib_table;
        vpp_prefix_t prefix;
    } vpp_sr_steer_t;

    typedef struct vpp_event_info_ {
	struct vpp_event_info_ *next;
	vpp_event_type_e type;
	vpp_event_data_t data;
    } vpp_event_info_t;

    typedef void (*vpp_event_free_fn)(vpp_event_info_t *);

    typedef struct vpp_event_queue_ {
	vpp_event_info_t *head;
	vpp_event_info_t **tail;
	vpp_event_free_fn free;
    } vpp_event_queue_t;

/* VTR config options for API support */
typedef enum
{
  L2_VTR_DISABLED,
  L2_VTR_PUSH_1,
  L2_VTR_PUSH_2,
  L2_VTR_POP_1,
  L2_VTR_POP_2,
  L2_VTR_TRANSLATE_1_1,
  L2_VTR_TRANSLATE_1_2,
  L2_VTR_TRANSLATE_2_1,
  L2_VTR_TRANSLATE_2_2
} vpp_l2_vtr_op_t;

/*
 * VLAN tagging type
 */
typedef enum
{
  VLAN_DOT1AD,
  VLAN_DOT1Q
} vpp_vlan_type_t;
typedef enum {
    VPP_API_PORT_TYPE_NORMAL = 0,
    VPP_API_PORT_TYPE_BVI = 1,
    VPP_API_PORT_TYPE_UU_FWD = 2,
} vpp_l2_port_type_t;

typedef enum {
    VPP_BD_FLAG_NONE = 0,
    VPP_BD_FLAG_LEARN = 1,
    VPP_BD_FLAG_FWD = 2,
    VPP_BD_FLAG_FLOOD = 4,
    VPP_BD_FLAG_UU_FLOOD = 8,
    VPP_BD_FLAG_ARP_TERM = 16,
    VPP_BD_FLAG_ARP_UFWD = 32,
} vpp_bd_flags_t;

typedef enum {
  VPP_BOND_API_MODE_ROUND_ROBIN = 1,
  VPP_BOND_API_MODE_ACTIVE_BACKUP = 2,
  VPP_BOND_API_MODE_XOR = 3,
  VPP_BOND_API_MODE_BROADCAST = 4,
  VPP_BOND_API_MODE_LACP = 5,
}  vpp_bond_mode;


typedef enum {
  VPP_BOND_API_LB_ALGO_L2 = 0,
  VPP_BOND_API_LB_ALGO_L34 = 1,
  VPP_BOND_API_LB_ALGO_L23 = 2,
  VPP_BOND_API_LB_ALGO_RR = 3,
  VPP_BOND_API_LB_ALGO_BC = 4,
  VPP_BOND_API_LB_ALGO_AB = 5,
}  vpp_bond_lb_algo;

    typedef struct  _vpp_vxlan_tunnel {
        vpp_ip_addr_t src_address;
        vpp_ip_addr_t dst_address;
        uint16_t      src_port;
        uint16_t      dst_port;
        uint32_t      vni;
        uint32_t      instance; /* If non-~0, specifies a custom dev instance */
        uint32_t      mcast_sw_if_index;
        uint32_t      encap_vrf_id;
        uint32_t      decap_next_index;
        bool          is_l3;
     } vpp_vxlan_tunnel_t;

    extern vpp_event_info_t * vpp_ev_dequeue();
    extern void vpp_ev_free(vpp_event_info_t *evp);

    extern int init_vpp_client();
    extern int refresh_interfaces_list();
    extern int configure_lcp_interface(const char *hwif_name, const char *hostif_name, bool is_add);
    extern int create_loopback_instance(const char *hwif_name, uint32_t instance);
    extern int delete_loopback(const char *hwif_name, uint32_t instance);
    extern int get_sw_if_idx(const char *ifname);
    extern int create_sub_interface(const char *hwif_name, uint32_t sub_id, uint16_t vlan_id);
    extern int delete_sub_interface(const char *hwif_name, uint32_t sub_id);
    extern int set_interface_vrf(const char *hwif_name, uint32_t sub_id, uint32_t vrf_id, bool is_ipv6);
    extern int interface_ip_address_add_del(const char *hw_ifname, vpp_ip_route_t *prefix, bool is_add);
    extern int interface_set_state (const char *hwif_name, bool is_up);
    extern int hw_interface_set_mtu(const char *hwif_name, uint32_t mtu);
    extern int sw_interface_set_mtu(const char *hwif_name, uint32_t mtu);
    extern int sw_interface_set_mac(const char *hwif_name, uint8_t *mac_address);
    extern int sw_interface_ip6_enable_disable(const char *hwif_name, bool enable);
    extern int ip_vrf_add(uint32_t vrf_id, const char *vrf_name, bool is_ipv6);
    extern int ip_vrf_del(uint32_t vrf_id, const char *vrf_name, bool is_ipv6);

    extern int ip4_nbr_add_del(const char *hwif_name, uint32_t sw_if_index, struct sockaddr_in *addr,
			       bool is_static, bool no_fib_entry, uint8_t *mac, bool is_add);
    extern int ip6_nbr_add_del(const char *hwif_name, uint32_t sw_if_index, struct sockaddr_in6 *addr,
			       bool is_static, bool no_fib_entry, uint8_t *mac, bool is_add);
    extern int ip_route_add_del(vpp_ip_route_t *prefix, bool is_add);
    extern int vpp_ip_flow_hash_set(uint32_t vrf_id, uint32_t mask, int addr_family);

    extern int vpp_acl_add_replace(vpp_acl_t *in_acl, uint32_t *acl_index, bool is_replace);
    extern int vpp_acl_del(uint32_t acl_index);
    extern int vpp_acl_interface_bind(const char *hwif_name, uint32_t acl_index,
				      bool is_input);
    extern int vpp_acl_interface_unbind(const char *hwif_name, uint32_t acl_index,
					bool is_input);
    extern int vpp_tunterm_acl_add_replace (uint32_t *tunterm_index, uint32_t count, vpp_tunterm_acl_t *acl);
    extern int vpp_tunterm_acl_del (uint32_t tunterm_index);
    extern int vpp_tunterm_acl_interface_add_del (uint32_t tunterm_index,
                                           bool is_bind, const char *hwif_name);
    extern int interface_get_state(const char *hwif_name, bool *link_is_up);
    extern int vpp_sync_for_events();
    extern int vpp_bridge_domain_add_del(uint32_t bridge_id, bool is_add);
    extern int set_sw_interface_l2_bridge(const char *hwif_name, uint32_t bridge_id, bool l2_enable, uint32_t port_type);
    extern int set_sw_interface_l2_bridge_by_index(uint32_t sw_if_index, uint32_t bridge_id, bool l2_enable, uint32_t port_type);
    extern int set_l2_interface_vlan_tag_rewrite(const char *hwif_name, uint32_t tag1, uint32_t tag2, uint32_t push_dot1q, uint32_t vtr_op);
    extern int bridge_domain_get_member_count (uint32_t bd_id, uint32_t *member_count);
    extern int create_bvi_interface(uint8_t *mac_address, uint32_t instance);
    extern int delete_bvi_interface(const char *hwif_name);
    extern int set_bridge_domain_flags(uint32_t bd_id, vpp_bd_flags_t flag, bool enable);
    extern int create_bond_interface(uint32_t bond_id, uint32_t mode, uint32_t lb, uint32_t *swif_idx);
    extern int delete_bond_interface(const char *hwif_name);
    extern int create_bond_member(uint32_t bond_sw_if_index, const char *hwif_name, bool is_passive, bool is_long_timeout);
    extern int delete_bond_member(const char * hwif_name);
    extern const char * vpp_get_swif_name(const uint32_t swif_idx);
    extern int l2fib_add_del(const char *hwif_name, const uint8_t *mac, uint32_t bd_id, bool is_add, bool is_static_mac);
    extern int l2fib_flush_all();
    extern int l2fib_flush_int(const char *hwif_name);
    extern int l2fib_flush_bd(uint32_t bd_id);
    extern int bfd_udp_add(bool multihop, const char *hwif_name, vpp_ip_addr_t *local_addr,
                           vpp_ip_addr_t *peer_addr, uint8_t detect_mult,
                           uint32_t desired_min_tx, uint32_t required_min_rx);
    extern int bfd_udp_del(bool multihop, const char *hwif_name, vpp_ip_addr_t *local_addr,
                           vpp_ip_addr_t *peer_addr);

    extern int vpp_vxlan_tunnel_add_del(vpp_vxlan_tunnel_t *tunnel, bool is_add,  uint32_t *sw_if_index);
    extern int vpp_ip_addr_t_to_string(vpp_ip_addr_t *ip_addr, char *buffer, size_t maxlen);
    extern int vpp_my_sid_entry_add_del(vpp_my_sid_entry_t *my_sid, bool is_del);
    extern int vpp_sidlist_add(vpp_sidlist_t *sidlist);
    extern int vpp_sidlist_del(vpp_ip_addr_t *bsid);
    extern int vpp_sr_steer_add_del(vpp_sr_steer_t *sr_steer, bool is_del);
    extern int vpp_sr_set_encap_source(vpp_ip_addr_t *encap_src);
#ifdef __cplusplus
}
#endif

#endif
