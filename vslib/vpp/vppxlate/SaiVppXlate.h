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

/*
 * VPP API error codes mirrored from vnet/error.h.
 * Use SAI_VPP_ERR_ prefix to avoid collision with the VPP enum names.
 * Verified at compile time via _Static_assert in SaiVppXlate.c.
 *
 * Keep in sync with foreach_vnet_error in vnet/error.h.
 */
#define foreach_sai_vpp_error                                                  \
  _(UNSPECIFIED, -1)                                                           \
  _(INVALID_SW_IF_INDEX, -2)                                                   \
  _(NO_SUCH_FIB, -3)                                                           \
  _(NO_SUCH_INNER_FIB, -4)                                                     \
  _(NO_SUCH_LABEL, -5)                                                         \
  _(NO_SUCH_ENTRY, -6)                                                         \
  _(INVALID_VALUE, -7)                                                         \
  _(INVALID_VALUE_2, -8)                                                       \
  _(UNIMPLEMENTED, -9)                                                         \
  _(INVALID_SW_IF_INDEX_2, -10)                                                \
  _(SYSCALL_ERROR_1, -11)                                                      \
  _(SYSCALL_ERROR_2, -12)                                                      \
  _(SYSCALL_ERROR_3, -13)                                                      \
  _(SYSCALL_ERROR_4, -14)                                                      \
  _(SYSCALL_ERROR_5, -15)                                                      \
  _(SYSCALL_ERROR_6, -16)                                                      \
  _(SYSCALL_ERROR_7, -17)                                                      \
  _(SYSCALL_ERROR_8, -18)                                                      \
  _(SYSCALL_ERROR_9, -19)                                                      \
  _(SYSCALL_ERROR_10, -20)                                                     \
  _(FEATURE_DISABLED, -30)                                                     \
  _(INVALID_REGISTRATION, -31)                                                 \
  _(NEXT_HOP_NOT_IN_FIB, -50)                                                  \
  _(UNKNOWN_DESTINATION, -51)                                                  \
  _(NO_PATHS_IN_ROUTE, -52)                                                    \
  _(NEXT_HOP_NOT_FOUND_MP, -53)                                                \
  _(NO_MATCHING_INTERFACE, -54)                                                \
  _(INVALID_VLAN, -55)                                                         \
  _(VLAN_ALREADY_EXISTS, -56)                                                  \
  _(INVALID_SRC_ADDRESS, -57)                                                  \
  _(INVALID_DST_ADDRESS, -58)                                                  \
  _(ADDRESS_LENGTH_MISMATCH, -59)                                              \
  _(ADDRESS_NOT_FOUND_FOR_INTERFACE, -60)                                      \
  _(ADDRESS_NOT_DELETABLE, -61)                                                \
  _(IP6_NOT_ENABLED, -62)                                                      \
  _(NO_SUCH_NODE, -63)                                                         \
  _(NO_SUCH_NODE2, -64)                                                        \
  _(NO_SUCH_TABLE, -65)                                                        \
  _(NO_SUCH_TABLE2, -66)                                                       \
  _(NO_SUCH_TABLE3, -67)                                                       \
  _(SUBIF_ALREADY_EXISTS, -68)                                                 \
  _(SUBIF_CREATE_FAILED, -69)                                                  \
  _(INVALID_MEMORY_SIZE, -70)                                                  \
  _(INVALID_INTERFACE, -71)                                                    \
  _(INVALID_VLAN_TAG_COUNT, -72)                                               \
  _(INVALID_ARGUMENT, -73)                                                     \
  _(UNEXPECTED_INTF_STATE, -74)                                                \
  _(TUNNEL_EXIST, -75)                                                         \
  _(INVALID_DECAP_NEXT, -76)                                                   \
  _(RESPONSE_NOT_READY, -77)                                                   \
  _(NOT_CONNECTED, -78)                                                        \
  _(IF_ALREADY_EXISTS, -79)                                                    \
  _(BOND_SLAVE_NOT_ALLOWED, -80)                                               \
  _(VALUE_EXIST, -81)                                                          \
  _(SAME_SRC_DST, -82)                                                        \
  _(IP6_MULTICAST_ADDRESS_NOT_PRESENT, -83)                                    \
  _(SR_POLICY_NAME_NOT_PRESENT, -84)                                           \
  _(NOT_RUNNING_AS_ROOT, -85)                                                  \
  _(ALREADY_CONNECTED, -86)                                                    \
  _(UNSUPPORTED_JNI_VERSION, -87)                                              \
  _(IP_PREFIX_INVALID, -88)                                                    \
  _(INVALID_WORKER, -89)                                                       \
  _(LISP_DISABLED, -90)                                                        \
  _(CLASSIFY_TABLE_NOT_FOUND, -91)                                             \
  _(INVALID_EID_TYPE, -92)                                                     \
  _(CANNOT_CREATE_PCAP_FILE, -93)                                              \
  _(INCORRECT_ADJACENCY_TYPE, -94)                                             \
  _(EXCEEDED_NUMBER_OF_RANGES_CAPACITY, -95)                                   \
  _(EXCEEDED_NUMBER_OF_PORTS_CAPACITY, -96)                                    \
  _(INVALID_ADDRESS_FAMILY, -97)                                               \
  _(INVALID_SUB_SW_IF_INDEX, -98)                                              \
  _(TABLE_TOO_BIG, -99)                                                        \
  _(CANNOT_ENABLE_DISABLE_FEATURE, -100)                                       \
  _(BFD_EEXIST, -101)                                                          \
  _(BFD_ENOENT, -102)                                                          \
  _(BFD_EINUSE, -103)                                                          \
  _(BFD_NOTSUPP, -104)                                                         \
  _(ADDRESS_IN_USE, -105)                                                      \
  _(ADDRESS_NOT_IN_USE, -106)                                                  \
  _(QUEUE_FULL, -107)                                                          \
  _(APP_UNSUPPORTED_CFG, -108)                                                 \
  _(URI_FIFO_CREATE_FAILED, -109)                                              \
  _(LISP_RLOC_LOCAL, -110)                                                     \
  _(BFD_EAGAIN, -111)                                                          \
  _(INVALID_GPE_MODE, -112)                                                    \
  _(LISP_GPE_ENTRIES_PRESENT, -113)                                            \
  _(ADDRESS_FOUND_FOR_INTERFACE, -114)                                         \
  _(SESSION_CONNECT, -115)                                                     \
  _(ENTRY_ALREADY_EXISTS, -116)                                                \
  _(SVM_SEGMENT_CREATE_FAIL, -117)                                             \
  _(APPLICATION_NOT_ATTACHED, -118)                                            \
  _(BD_ALREADY_EXISTS, -119)                                                   \
  _(BD_IN_USE, -120)                                                           \
  _(BD_NOT_MODIFIABLE, -121)                                                   \
  _(BD_ID_EXCEED_MAX, -122)                                                    \
  _(SUBIF_DOESNT_EXIST, -123)                                                  \
  _(L2_MACS_EVENT_CLINET_PRESENT, -124)                                        \
  _(INVALID_QUEUE, -125)                                                       \
  _(UNSUPPORTED, -126)                                                         \
  _(DUPLICATE_IF_ADDRESS, -127)                                                \
  _(APP_INVALID_NS, -128)                                                      \
  _(APP_WRONG_NS_SECRET, -129)                                                 \
  _(APP_CONNECT_SCOPE, -130)                                                   \
  _(APP_ALREADY_ATTACHED, -131)                                                \
  _(SESSION_REDIRECT, -132)                                                    \
  _(ILLEGAL_NAME, -133)                                                        \
  _(NO_NAME_SERVERS, -134)                                                     \
  _(NAME_SERVER_NOT_FOUND, -135)                                               \
  _(NAME_RESOLUTION_NOT_ENABLED, -136)                                         \
  _(NAME_SERVER_FORMAT_ERROR, -137)                                            \
  _(NAME_SERVER_NO_SUCH_NAME, -138)                                            \
  _(NAME_SERVER_NO_ADDRESSES, -139)                                            \
  _(NAME_SERVER_NEXT_SERVER, -140)                                             \
  _(APP_CONNECT_FILTERED, -141)                                                \
  _(ACL_IN_USE_INBOUND, -142)                                                  \
  _(ACL_IN_USE_OUTBOUND, -143)                                                 \
  _(INIT_FAILED, -144)                                                         \
  _(NETLINK_ERROR, -145)                                                       \
  _(BIER_BSL_UNSUP, -146)                                                      \
  _(INSTANCE_IN_USE, -147)                                                     \
  _(INVALID_SESSION_ID, -148)                                                  \
  _(ACL_IN_USE_BY_LOOKUP_CONTEXT, -149)                                        \
  _(INVALID_VALUE_3, -150)                                                     \
  _(NON_ETHERNET, -151)                                                        \
  _(BD_ALREADY_HAS_BVI, -152)                                                  \
  _(INVALID_PROTOCOL, -153)                                                    \
  _(INVALID_ALGORITHM, -154)                                                   \
  _(RSRC_IN_USE, -155)                                                         \
  _(KEY_LENGTH, -156)                                                          \
  _(FIB_PATH_UNSUPPORTED_NH_PROTO, -157)                                       \
  _(API_ENDIAN_FAILED, -159)                                                   \
  _(NO_CHANGE, -160)                                                           \
  _(MISSING_CERT_KEY, -161)                                                    \
  _(LIMIT_EXCEEDED, -162)                                                      \
  _(IKE_NO_PORT, -163)                                                         \
  _(UDP_PORT_TAKEN, -164)                                                      \
  _(EAGAIN, -165)                                                              \
  _(INVALID_VALUE_4, -166)                                                     \
  _(BUSY, -167)                                                                \
  _(BUG, -168)                                                                 \
  _(FEATURE_ALREADY_DISABLED, -169)                                            \
  _(FEATURE_ALREADY_ENABLED, -170)                                             \
  _(INVALID_PREFIX_LENGTH, -171)

/* Generate SAI_VPP_ERR_<NAME> defines */
#define _(a, b) static const int SAI_VPP_ERR_##a = (b);
foreach_sai_vpp_error
#undef _

    static inline const char *sai_vpp_err_to_string(int err)
    {
        switch (err) {
#define _(a, b) case (b): return #a;
        foreach_sai_vpp_error
#undef _
        default: return "UNKNOWN";
        }
    }

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
    extern int interface_ip_address_del_all(const char *hwif_name);
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
