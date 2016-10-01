#include "saiserialize.h"

#include <vector>

sai_serialization_map_t g_serialization_map = sai_get_serialization_map();
sai_object_type_to_string_map_t g_object_type_map = sai_get_object_type_map();

sai_serialization_map_t sai_get_serialization_map()
{
    SWSS_LOG_ENTER();

    sai_serialization_map_t map;

    map[SAI_OBJECT_TYPE_BUFFER_POOL][SAI_BUFFER_POOL_ATTR_SHARED_SIZE] = SAI_SERIALIZATION_TYPE_UINT32;
    map[SAI_OBJECT_TYPE_BUFFER_POOL][SAI_BUFFER_POOL_ATTR_TYPE] = SAI_SERIALIZATION_TYPE_INT32;
    map[SAI_OBJECT_TYPE_BUFFER_POOL][SAI_BUFFER_POOL_ATTR_SIZE] = SAI_SERIALIZATION_TYPE_UINT32;
    map[SAI_OBJECT_TYPE_BUFFER_POOL][SAI_BUFFER_POOL_ATTR_TH_MODE] = SAI_SERIALIZATION_TYPE_INT32;

    map[SAI_OBJECT_TYPE_BUFFER_PROFILE][SAI_BUFFER_PROFILE_ATTR_POOL_ID] = SAI_SERIALIZATION_TYPE_OBJECT_ID;
    map[SAI_OBJECT_TYPE_BUFFER_PROFILE][SAI_BUFFER_PROFILE_ATTR_BUFFER_SIZE] = SAI_SERIALIZATION_TYPE_UINT32;
    map[SAI_OBJECT_TYPE_BUFFER_PROFILE][SAI_BUFFER_PROFILE_ATTR_SHARED_DYNAMIC_TH] = SAI_SERIALIZATION_TYPE_INT8;
    map[SAI_OBJECT_TYPE_BUFFER_PROFILE][SAI_BUFFER_PROFILE_ATTR_SHARED_STATIC_TH] = SAI_SERIALIZATION_TYPE_UINT32;
    map[SAI_OBJECT_TYPE_BUFFER_PROFILE][SAI_BUFFER_PROFILE_ATTR_XOFF_TH] = SAI_SERIALIZATION_TYPE_UINT32;
    map[SAI_OBJECT_TYPE_BUFFER_PROFILE][SAI_BUFFER_PROFILE_ATTR_XON_TH] = SAI_SERIALIZATION_TYPE_UINT32;

    map[SAI_OBJECT_TYPE_ACL_TABLE][SAI_ACL_TABLE_ATTR_PRIORITY] = SAI_SERIALIZATION_TYPE_UINT32;

    map[SAI_OBJECT_TYPE_QOS_MAPS][SAI_QOS_MAP_ATTR_TYPE] = SAI_SERIALIZATION_TYPE_INT32;
    map[SAI_OBJECT_TYPE_QOS_MAPS][SAI_QOS_MAP_ATTR_MAP_TO_VALUE_LIST] = SAI_SERIALIZATION_TYPE_QOS_MAP_LIST;

    map[SAI_OBJECT_TYPE_WRED][SAI_WRED_ATTR_GREEN_ENABLE] = SAI_SERIALIZATION_TYPE_BOOL;
    map[SAI_OBJECT_TYPE_WRED][SAI_WRED_ATTR_GREEN_MIN_THRESHOLD] = SAI_SERIALIZATION_TYPE_UINT32;
    map[SAI_OBJECT_TYPE_WRED][SAI_WRED_ATTR_GREEN_MAX_THRESHOLD] = SAI_SERIALIZATION_TYPE_UINT32;
    map[SAI_OBJECT_TYPE_WRED][SAI_WRED_ATTR_GREEN_DROP_PROBABILITY] = SAI_SERIALIZATION_TYPE_UINT32;
    map[SAI_OBJECT_TYPE_WRED][SAI_WRED_ATTR_YELLOW_ENABLE] = SAI_SERIALIZATION_TYPE_BOOL;
    map[SAI_OBJECT_TYPE_WRED][SAI_WRED_ATTR_YELLOW_MIN_THRESHOLD] = SAI_SERIALIZATION_TYPE_UINT32;
    map[SAI_OBJECT_TYPE_WRED][SAI_WRED_ATTR_YELLOW_MAX_THRESHOLD] = SAI_SERIALIZATION_TYPE_UINT32;
    map[SAI_OBJECT_TYPE_WRED][SAI_WRED_ATTR_YELLOW_DROP_PROBABILITY] = SAI_SERIALIZATION_TYPE_UINT32;
    map[SAI_OBJECT_TYPE_WRED][SAI_WRED_ATTR_RED_ENABLE] = SAI_SERIALIZATION_TYPE_BOOL;
    map[SAI_OBJECT_TYPE_WRED][SAI_WRED_ATTR_RED_MIN_THRESHOLD] = SAI_SERIALIZATION_TYPE_UINT32;
    map[SAI_OBJECT_TYPE_WRED][SAI_WRED_ATTR_RED_MAX_THRESHOLD] = SAI_SERIALIZATION_TYPE_UINT32;
    map[SAI_OBJECT_TYPE_WRED][SAI_WRED_ATTR_RED_DROP_PROBABILITY] = SAI_SERIALIZATION_TYPE_UINT32;
    map[SAI_OBJECT_TYPE_WRED][SAI_WRED_ATTR_WEIGHT] = SAI_SERIALIZATION_TYPE_UINT8;
    map[SAI_OBJECT_TYPE_WRED][SAI_WRED_ATTR_ECN_MARK_MODE] = SAI_SERIALIZATION_TYPE_INT32;

    map[SAI_OBJECT_TYPE_PORT][SAI_PORT_ATTR_SPEED] = SAI_SERIALIZATION_TYPE_UINT32;
    map[SAI_OBJECT_TYPE_PORT][SAI_PORT_ATTR_ADMIN_STATE] = SAI_SERIALIZATION_TYPE_BOOL;
    map[SAI_OBJECT_TYPE_PORT][SAI_PORT_ATTR_OPER_STATUS] = SAI_SERIALIZATION_TYPE_INT32;
    map[SAI_OBJECT_TYPE_PORT][SAI_PORT_ATTR_PORT_VLAN_ID] = SAI_SERIALIZATION_TYPE_UINT16;
    map[SAI_OBJECT_TYPE_PORT][SAI_PORT_ATTR_FDB_LEARNING] = SAI_SERIALIZATION_TYPE_INT32;
    map[SAI_OBJECT_TYPE_PORT][SAI_PORT_ATTR_HW_LANE_LIST] = SAI_SERIALIZATION_TYPE_UINT32_LIST;
    map[SAI_OBJECT_TYPE_PORT][SAI_PORT_ATTR_QOS_DSCP_TO_TC_MAP] = SAI_SERIALIZATION_TYPE_OBJECT_ID;
    map[SAI_OBJECT_TYPE_PORT][SAI_PORT_ATTR_QOS_TC_TO_QUEUE_MAP] = SAI_SERIALIZATION_TYPE_OBJECT_ID;
    map[SAI_OBJECT_TYPE_PORT][SAI_PORT_ATTR_QOS_TC_TO_PRIORITY_GROUP_MAP] = SAI_SERIALIZATION_TYPE_OBJECT_ID;
    map[SAI_OBJECT_TYPE_PORT][SAI_PORT_ATTR_QOS_PFC_PRIORITY_TO_PRIORITY_GROUP_MAP] = SAI_SERIALIZATION_TYPE_OBJECT_ID;
    map[SAI_OBJECT_TYPE_PORT][SAI_PORT_ATTR_QOS_PFC_PRIORITY_TO_QUEUE_MAP] = SAI_SERIALIZATION_TYPE_OBJECT_ID;
    map[SAI_OBJECT_TYPE_PORT][SAI_PORT_ATTR_QOS_INGRESS_BUFFER_PROFILE_LIST] = SAI_SERIALIZATION_TYPE_OBJECT_LIST;
    map[SAI_OBJECT_TYPE_PORT][SAI_PORT_ATTR_QOS_EGRESS_BUFFER_PROFILE_LIST] = SAI_SERIALIZATION_TYPE_OBJECT_LIST;
    map[SAI_OBJECT_TYPE_PORT][SAI_PORT_ATTR_NUMBER_OF_PRIORITY_GROUPS] = SAI_SERIALIZATION_TYPE_UINT32;
    map[SAI_OBJECT_TYPE_PORT][SAI_PORT_ATTR_PRIORITY_GROUP_LIST] = SAI_SERIALIZATION_TYPE_OBJECT_LIST;
    map[SAI_OBJECT_TYPE_PORT][SAI_PORT_ATTR_QOS_NUMBER_OF_QUEUES] = SAI_SERIALIZATION_TYPE_UINT32;
    map[SAI_OBJECT_TYPE_PORT][SAI_PORT_ATTR_QOS_QUEUE_LIST] = SAI_SERIALIZATION_TYPE_OBJECT_LIST;
    map[SAI_OBJECT_TYPE_PORT][SAI_PORT_ATTR_QOS_NUMBER_OF_SCHEDULER_GROUPS] = SAI_SERIALIZATION_TYPE_UINT32;
    map[SAI_OBJECT_TYPE_PORT][SAI_PORT_ATTR_QOS_SCHEDULER_GROUP_LIST] = SAI_SERIALIZATION_TYPE_OBJECT_LIST;
    map[SAI_OBJECT_TYPE_PORT][SAI_PORT_ATTR_PRIORITY_FLOW_CONTROL] = SAI_SERIALIZATION_TYPE_UINT8;

    map[SAI_OBJECT_TYPE_SCHEDULER][SAI_SCHEDULER_ATTR_SCHEDULING_ALGORITHM] = SAI_SERIALIZATION_TYPE_UINT32;
    map[SAI_OBJECT_TYPE_SCHEDULER][SAI_SCHEDULER_ATTR_SCHEDULING_WEIGHT] = SAI_SERIALIZATION_TYPE_UINT8;
    map[SAI_OBJECT_TYPE_SCHEDULER][SAI_SCHEDULER_ATTR_SHAPER_TYPE] = SAI_SERIALIZATION_TYPE_UINT32;
    map[SAI_OBJECT_TYPE_SCHEDULER][SAI_SCHEDULER_ATTR_MIN_BANDWIDTH_RATE] = SAI_SERIALIZATION_TYPE_UINT64;
    map[SAI_OBJECT_TYPE_SCHEDULER][SAI_SCHEDULER_ATTR_MIN_BANDWIDTH_BURST_RATE] = SAI_SERIALIZATION_TYPE_UINT64;
    map[SAI_OBJECT_TYPE_SCHEDULER][SAI_SCHEDULER_ATTR_MAX_BANDWIDTH_RATE] = SAI_SERIALIZATION_TYPE_UINT64;
    map[SAI_OBJECT_TYPE_SCHEDULER][SAI_SCHEDULER_ATTR_MAX_BANDWIDTH_BURST_RATE] = SAI_SERIALIZATION_TYPE_UINT64;

    map[SAI_OBJECT_TYPE_SCHEDULER_GROUP][SAI_SCHEDULER_GROUP_ATTR_CHILD_COUNT] = SAI_SERIALIZATION_TYPE_UINT32;
    map[SAI_OBJECT_TYPE_SCHEDULER_GROUP][SAI_SCHEDULER_GROUP_ATTR_CHILD_LIST] = SAI_SERIALIZATION_TYPE_OBJECT_LIST;
    map[SAI_OBJECT_TYPE_SCHEDULER_GROUP][SAI_SCHEDULER_GROUP_ATTR_SCHEDULER_PROFILE_ID] = SAI_SERIALIZATION_TYPE_OBJECT_ID;

    map[SAI_OBJECT_TYPE_PRIORITY_GROUP][SAI_INGRESS_PRIORITY_GROUP_ATTR_BUFFER_PROFILE] = SAI_SERIALIZATION_TYPE_OBJECT_ID;

    map[SAI_OBJECT_TYPE_QUEUE][SAI_QUEUE_ATTR_WRED_PROFILE_ID] = SAI_SERIALIZATION_TYPE_OBJECT_ID;
    map[SAI_OBJECT_TYPE_QUEUE][SAI_QUEUE_ATTR_BUFFER_PROFILE_ID] = SAI_SERIALIZATION_TYPE_OBJECT_ID;
    map[SAI_OBJECT_TYPE_QUEUE][SAI_QUEUE_ATTR_SCHEDULER_PROFILE_ID] = SAI_SERIALIZATION_TYPE_OBJECT_ID;

    map[SAI_OBJECT_TYPE_NEXT_HOP][SAI_NEXT_HOP_ATTR_TYPE] = SAI_SERIALIZATION_TYPE_INT32;
    map[SAI_OBJECT_TYPE_NEXT_HOP][SAI_NEXT_HOP_ATTR_IP] = SAI_SERIALIZATION_TYPE_IP_ADDRESS;
    map[SAI_OBJECT_TYPE_NEXT_HOP][SAI_NEXT_HOP_ATTR_ROUTER_INTERFACE_ID] = SAI_SERIALIZATION_TYPE_OBJECT_ID;

    map[SAI_OBJECT_TYPE_NEXT_HOP_GROUP][SAI_NEXT_HOP_GROUP_ATTR_NEXT_HOP_COUNT] = SAI_SERIALIZATION_TYPE_UINT32;
    map[SAI_OBJECT_TYPE_NEXT_HOP_GROUP][SAI_NEXT_HOP_GROUP_ATTR_TYPE] = SAI_SERIALIZATION_TYPE_INT32;
    map[SAI_OBJECT_TYPE_NEXT_HOP_GROUP][SAI_NEXT_HOP_GROUP_ATTR_NEXT_HOP_LIST] = SAI_SERIALIZATION_TYPE_OBJECT_LIST;

    map[SAI_OBJECT_TYPE_ROUTER_INTERFACE][SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID] = SAI_SERIALIZATION_TYPE_OBJECT_ID;
    map[SAI_OBJECT_TYPE_ROUTER_INTERFACE][SAI_ROUTER_INTERFACE_ATTR_TYPE] = SAI_SERIALIZATION_TYPE_INT32;
    map[SAI_OBJECT_TYPE_ROUTER_INTERFACE][SAI_ROUTER_INTERFACE_ATTR_SRC_MAC_ADDRESS] = SAI_SERIALIZATION_TYPE_MAC;
    map[SAI_OBJECT_TYPE_ROUTER_INTERFACE][SAI_ROUTER_INTERFACE_ATTR_VLAN_ID] = SAI_SERIALIZATION_TYPE_UINT16;
    map[SAI_OBJECT_TYPE_ROUTER_INTERFACE][SAI_ROUTER_INTERFACE_ATTR_PORT_ID] = SAI_SERIALIZATION_TYPE_OBJECT_ID;

    map[SAI_OBJECT_TYPE_HOST_INTERFACE][SAI_HOSTIF_ATTR_TYPE] = SAI_SERIALIZATION_TYPE_INT32;
    map[SAI_OBJECT_TYPE_HOST_INTERFACE][SAI_HOSTIF_ATTR_RIF_OR_PORT_ID] = SAI_SERIALIZATION_TYPE_OBJECT_ID;
    map[SAI_OBJECT_TYPE_HOST_INTERFACE][SAI_HOSTIF_ATTR_NAME] = SAI_SERIALIZATION_TYPE_CHARDATA;
    map[SAI_OBJECT_TYPE_HOST_INTERFACE][SAI_HOSTIF_ATTR_OPER_STATUS] = SAI_SERIALIZATION_TYPE_BOOL;

    map[SAI_OBJECT_TYPE_NEIGHBOR][SAI_NEIGHBOR_ATTR_DST_MAC_ADDRESS] = SAI_SERIALIZATION_TYPE_MAC;

    map[SAI_OBJECT_TYPE_ROUTE][SAI_ROUTE_ATTR_PACKET_ACTION] = SAI_SERIALIZATION_TYPE_INT32;
    map[SAI_OBJECT_TYPE_ROUTE][SAI_ROUTE_ATTR_TRAP_PRIORITY] = SAI_SERIALIZATION_TYPE_UINT8;
    map[SAI_OBJECT_TYPE_ROUTE][SAI_ROUTE_ATTR_NEXT_HOP_ID] = SAI_SERIALIZATION_TYPE_OBJECT_ID;
    map[SAI_OBJECT_TYPE_ROUTE][SAI_ROUTE_ATTR_META_DATA] = SAI_SERIALIZATION_TYPE_UINT32;

    map[SAI_OBJECT_TYPE_SWITCH][SAI_SWITCH_ATTR_SWITCHING_MODE] = SAI_SERIALIZATION_TYPE_BOOL;
    map[SAI_OBJECT_TYPE_SWITCH][SAI_SWITCH_ATTR_PORT_NUMBER] = SAI_SERIALIZATION_TYPE_UINT32;
    map[SAI_OBJECT_TYPE_SWITCH][SAI_SWITCH_ATTR_PORT_LIST] = SAI_SERIALIZATION_TYPE_OBJECT_LIST;
    map[SAI_OBJECT_TYPE_SWITCH][SAI_SWITCH_ATTR_PORT_MAX_MTU] = SAI_SERIALIZATION_TYPE_UINT32;
    map[SAI_OBJECT_TYPE_SWITCH][SAI_SWITCH_ATTR_CPU_PORT] = SAI_SERIALIZATION_TYPE_OBJECT_ID;
    map[SAI_OBJECT_TYPE_SWITCH][SAI_SWITCH_ATTR_SRC_MAC_ADDRESS] = SAI_SERIALIZATION_TYPE_MAC;
    map[SAI_OBJECT_TYPE_SWITCH][SAI_SWITCH_ATTR_DEFAULT_VIRTUAL_ROUTER_ID] = SAI_SERIALIZATION_TYPE_OBJECT_ID;
    map[SAI_OBJECT_TYPE_SWITCH][SAI_SWITCH_ATTR_EGRESS_BUFFER_POOL_NUM] = SAI_SERIALIZATION_TYPE_UINT32;
    map[SAI_OBJECT_TYPE_SWITCH][SAI_SWITCH_ATTR_INGRESS_BUFFER_POOL_NUM] = SAI_SERIALIZATION_TYPE_UINT32;
    map[SAI_OBJECT_TYPE_SWITCH][SAI_SWITCH_ATTR_QOS_MAX_NUMBER_OF_CHILDS_PER_SCHEDULER_GROUP] = SAI_SERIALIZATION_TYPE_INT32;
    map[SAI_OBJECT_TYPE_SWITCH][SAI_SWITCH_ATTR_DEFAULT_TRAP_GROUP] = SAI_SERIALIZATION_TYPE_OBJECT_ID;

#ifdef BRCMSAI
    map[SAI_OBJECT_TYPE_SWITCH][SAI_SWITCH_ATTR_SWITCH_SHELL_ENABLE] = SAI_SERIALIZATION_TYPE_BOOL;
#endif

    map[SAI_OBJECT_TYPE_FDB][SAI_FDB_ENTRY_ATTR_TYPE] = SAI_SERIALIZATION_TYPE_INT32;
    map[SAI_OBJECT_TYPE_FDB][SAI_FDB_ENTRY_ATTR_PORT_ID] = SAI_SERIALIZATION_TYPE_OBJECT_ID;

    map[SAI_OBJECT_TYPE_VLAN][SAI_VLAN_ATTR_MAX_LEARNED_ADDRESSES] = SAI_SERIALIZATION_TYPE_UINT32;
    map[SAI_OBJECT_TYPE_VLAN][SAI_VLAN_ATTR_MEMBER_LIST] = SAI_SERIALIZATION_TYPE_OBJECT_LIST;

    map[SAI_OBJECT_TYPE_VLAN_MEMBER][SAI_VLAN_MEMBER_ATTR_VLAN_ID] = SAI_SERIALIZATION_TYPE_UINT16;
    map[SAI_OBJECT_TYPE_VLAN_MEMBER][SAI_VLAN_MEMBER_ATTR_PORT_ID] = SAI_SERIALIZATION_TYPE_OBJECT_ID;

    map[SAI_OBJECT_TYPE_TRAP][SAI_HOSTIF_TRAP_ATTR_PACKET_ACTION] = SAI_SERIALIZATION_TYPE_INT32;
    map[SAI_OBJECT_TYPE_TRAP][SAI_HOSTIF_TRAP_ATTR_TRAP_PRIORITY] = SAI_SERIALIZATION_TYPE_UINT32;
    map[SAI_OBJECT_TYPE_TRAP][SAI_HOSTIF_TRAP_ATTR_TRAP_CHANNEL] = SAI_SERIALIZATION_TYPE_INT32;
    map[SAI_OBJECT_TYPE_TRAP][SAI_HOSTIF_TRAP_ATTR_FD] = SAI_SERIALIZATION_TYPE_OBJECT_ID;
    map[SAI_OBJECT_TYPE_TRAP][SAI_HOSTIF_TRAP_ATTR_PORT_LIST] = SAI_SERIALIZATION_TYPE_OBJECT_LIST;
    map[SAI_OBJECT_TYPE_TRAP][SAI_HOSTIF_TRAP_ATTR_TRAP_GROUP] = SAI_SERIALIZATION_TYPE_OBJECT_ID;

    map[SAI_OBJECT_TYPE_LAG_MEMBER][SAI_LAG_MEMBER_ATTR_LAG_ID] = SAI_SERIALIZATION_TYPE_OBJECT_ID;
    map[SAI_OBJECT_TYPE_LAG_MEMBER][SAI_LAG_MEMBER_ATTR_PORT_ID] = SAI_SERIALIZATION_TYPE_OBJECT_ID;

    map[SAI_OBJECT_TYPE_TUNNEL][SAI_TUNNEL_ATTR_TYPE] = SAI_SERIALIZATION_TYPE_INT32;
    map[SAI_OBJECT_TYPE_TUNNEL][SAI_TUNNEL_ATTR_OVERLAY_INTERFACE] = SAI_SERIALIZATION_TYPE_OBJECT_ID;
    map[SAI_OBJECT_TYPE_TUNNEL][SAI_TUNNEL_ATTR_UNDERLAY_INTERFACE] = SAI_SERIALIZATION_TYPE_OBJECT_ID;
    map[SAI_OBJECT_TYPE_TUNNEL][SAI_TUNNEL_ATTR_DECAP_ECN_MODE] = SAI_SERIALIZATION_TYPE_INT32;
    map[SAI_OBJECT_TYPE_TUNNEL][SAI_TUNNEL_ATTR_DECAP_TTL_MODE] = SAI_SERIALIZATION_TYPE_INT32;
    map[SAI_OBJECT_TYPE_TUNNEL][SAI_TUNNEL_ATTR_DECAP_DSCP_MODE] = SAI_SERIALIZATION_TYPE_INT32;

    map[SAI_OBJECT_TYPE_TUNNEL_TABLE_ENTRY][SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_VR_ID] = SAI_SERIALIZATION_TYPE_OBJECT_ID;
    map[SAI_OBJECT_TYPE_TUNNEL_TABLE_ENTRY][SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_TYPE] = SAI_SERIALIZATION_TYPE_INT32;
    map[SAI_OBJECT_TYPE_TUNNEL_TABLE_ENTRY][SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_DST_IP] = SAI_SERIALIZATION_TYPE_IP_ADDRESS;
    map[SAI_OBJECT_TYPE_TUNNEL_TABLE_ENTRY][SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_TUNNEL_TYPE] = SAI_SERIALIZATION_TYPE_INT32;
    map[SAI_OBJECT_TYPE_TUNNEL_TABLE_ENTRY][SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_ACTION_TUNNEL_ID] = SAI_SERIALIZATION_TYPE_OBJECT_ID;

    map[SAI_OBJECT_TYPE_POLICER][SAI_POLICER_ATTR_METER_TYPE] = SAI_SERIALIZATION_TYPE_UINT32;
    map[SAI_OBJECT_TYPE_POLICER][SAI_POLICER_ATTR_MODE] = SAI_SERIALIZATION_TYPE_UINT32;
    map[SAI_OBJECT_TYPE_POLICER][SAI_POLICER_ATTR_COLOR_SOURCE] = SAI_SERIALIZATION_TYPE_UINT32;
    map[SAI_OBJECT_TYPE_POLICER][SAI_POLICER_ATTR_CBS] = SAI_SERIALIZATION_TYPE_UINT64;
    map[SAI_OBJECT_TYPE_POLICER][SAI_POLICER_ATTR_CIR] = SAI_SERIALIZATION_TYPE_UINT64;
    map[SAI_OBJECT_TYPE_POLICER][SAI_POLICER_ATTR_PBS] = SAI_SERIALIZATION_TYPE_UINT64;
    map[SAI_OBJECT_TYPE_POLICER][SAI_POLICER_ATTR_PIR] = SAI_SERIALIZATION_TYPE_UINT64;
    map[SAI_OBJECT_TYPE_POLICER][SAI_POLICER_ATTR_GREEN_PACKET_ACTION] = SAI_SERIALIZATION_TYPE_UINT32;
    map[SAI_OBJECT_TYPE_POLICER][SAI_POLICER_ATTR_YELLOW_PACKET_ACTION] = SAI_SERIALIZATION_TYPE_UINT32;
    map[SAI_OBJECT_TYPE_POLICER][SAI_POLICER_ATTR_RED_PACKET_ACTION] = SAI_SERIALIZATION_TYPE_UINT32;
    map[SAI_OBJECT_TYPE_POLICER][SAI_POLICER_ATTR_ENABLE_COUNTER_LIST] = SAI_SERIALIZATION_TYPE_INT32_LIST;

    map[SAI_OBJECT_TYPE_TRAP_GROUP][SAI_HOSTIF_TRAP_GROUP_ATTR_ADMIN_STATE] = SAI_SERIALIZATION_TYPE_BOOL;
    map[SAI_OBJECT_TYPE_TRAP_GROUP][SAI_HOSTIF_TRAP_GROUP_ATTR_QUEUE] = SAI_SERIALIZATION_TYPE_UINT32;
    map[SAI_OBJECT_TYPE_TRAP_GROUP][SAI_HOSTIF_TRAP_GROUP_ATTR_POLICER] = SAI_SERIALIZATION_TYPE_OBJECT_ID;

    map[SAI_OBJECT_TYPE_TUNNEL_MAP][SAI_TUNNEL_MAP_ATTR_TYPE] = SAI_SERIALIZATION_TYPE_INT32;
    map[SAI_OBJECT_TYPE_TUNNEL_MAP][SAI_TUNNEL_MAP_ATTR_MAP_TO_VALUE_LIST] = SAI_SERIALIZATION_TYPE_TUNNEL_MAP_LIST;

    map[SAI_OBJECT_TYPE_TUNNEL][SAI_TUNNEL_ATTR_TYPE] = SAI_SERIALIZATION_TYPE_INT32;
    map[SAI_OBJECT_TYPE_TUNNEL][SAI_TUNNEL_ATTR_UNDERLAY_INTERFACE] = SAI_SERIALIZATION_TYPE_OBJECT_ID;
    map[SAI_OBJECT_TYPE_TUNNEL][SAI_TUNNEL_ATTR_OVERLAY_INTERFACE] = SAI_SERIALIZATION_TYPE_OBJECT_ID;
    map[SAI_OBJECT_TYPE_TUNNEL][SAI_TUNNEL_ATTR_ENCAP_SRC_IP] = SAI_SERIALIZATION_TYPE_IP_ADDRESS;
    map[SAI_OBJECT_TYPE_TUNNEL][SAI_TUNNEL_ATTR_ENCAP_TTL_MODE] = SAI_SERIALIZATION_TYPE_INT32;
    map[SAI_OBJECT_TYPE_TUNNEL][SAI_TUNNEL_ATTR_ENCAP_TTL_VAL] = SAI_SERIALIZATION_TYPE_UINT8;
    map[SAI_OBJECT_TYPE_TUNNEL][SAI_TUNNEL_ATTR_ENCAP_DSCP_MODE] = SAI_SERIALIZATION_TYPE_INT32;
    map[SAI_OBJECT_TYPE_TUNNEL][SAI_TUNNEL_ATTR_ENCAP_DSCP_VAL] = SAI_SERIALIZATION_TYPE_UINT8;
    map[SAI_OBJECT_TYPE_TUNNEL][SAI_TUNNEL_ATTR_ENCAP_GRE_KEY_VALID] = SAI_SERIALIZATION_TYPE_BOOL;
    map[SAI_OBJECT_TYPE_TUNNEL][SAI_TUNNEL_ATTR_ENCAP_GRE_KEY] = SAI_SERIALIZATION_TYPE_UINT32;
    map[SAI_OBJECT_TYPE_TUNNEL][SAI_TUNNEL_ATTR_ENCAP_ECN_MODE] = SAI_SERIALIZATION_TYPE_INT32;
    map[SAI_OBJECT_TYPE_TUNNEL][SAI_TUNNEL_ATTR_ENCAP_MAPPERS] = SAI_SERIALIZATION_TYPE_OBJECT_LIST;
    map[SAI_OBJECT_TYPE_TUNNEL][SAI_TUNNEL_ATTR_DECAP_ECN_MODE] = SAI_SERIALIZATION_TYPE_INT32;
    map[SAI_OBJECT_TYPE_TUNNEL][SAI_TUNNEL_ATTR_DECAP_MAPPERS] = SAI_SERIALIZATION_TYPE_OBJECT_LIST;
    map[SAI_OBJECT_TYPE_TUNNEL][SAI_TUNNEL_ATTR_DECAP_TTL_MODE] = SAI_SERIALIZATION_TYPE_INT32;
    map[SAI_OBJECT_TYPE_TUNNEL][SAI_TUNNEL_ATTR_DECAP_DSCP_MODE] = SAI_SERIALIZATION_TYPE_INT32;

    map[SAI_OBJECT_TYPE_TUNNEL_TABLE_ENTRY][SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_VR_ID] = SAI_SERIALIZATION_TYPE_OBJECT_ID;
    map[SAI_OBJECT_TYPE_TUNNEL_TABLE_ENTRY][SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_TYPE] = SAI_SERIALIZATION_TYPE_INT32;
    map[SAI_OBJECT_TYPE_TUNNEL_TABLE_ENTRY][SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_DST_IP] = SAI_SERIALIZATION_TYPE_IP_ADDRESS;
    map[SAI_OBJECT_TYPE_TUNNEL_TABLE_ENTRY][SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_SRC_IP] = SAI_SERIALIZATION_TYPE_IP_ADDRESS;
    map[SAI_OBJECT_TYPE_TUNNEL_TABLE_ENTRY][SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_TUNNEL_TYPE] = SAI_SERIALIZATION_TYPE_INT32;
    map[SAI_OBJECT_TYPE_TUNNEL_TABLE_ENTRY][SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_ACTION_TUNNEL_ID] = SAI_SERIALIZATION_TYPE_OBJECT_ID;
    return map;
}

sai_object_type_to_string_map_t sai_get_object_type_map()
{
    SWSS_LOG_ENTER();

    sai_object_type_to_string_map_t map;

    map[SAI_OBJECT_TYPE_NULL] = TO_STR(SAI_OBJECT_TYPE_NULL);
    map[SAI_OBJECT_TYPE_PORT] = TO_STR(SAI_OBJECT_TYPE_PORT);
    map[SAI_OBJECT_TYPE_LAG] = TO_STR(SAI_OBJECT_TYPE_LAG);
    map[SAI_OBJECT_TYPE_VIRTUAL_ROUTER] = TO_STR(SAI_OBJECT_TYPE_VIRTUAL_ROUTER);
    map[SAI_OBJECT_TYPE_NEXT_HOP] = TO_STR(SAI_OBJECT_TYPE_NEXT_HOP);
    map[SAI_OBJECT_TYPE_NEXT_HOP_GROUP] = TO_STR(SAI_OBJECT_TYPE_NEXT_HOP_GROUP);
    map[SAI_OBJECT_TYPE_ROUTER_INTERFACE] = TO_STR(SAI_OBJECT_TYPE_ROUTER_INTERFACE);
    map[SAI_OBJECT_TYPE_ACL_TABLE] = TO_STR(SAI_OBJECT_TYPE_ACL_TABLE);
    map[SAI_OBJECT_TYPE_ACL_ENTRY] = TO_STR(SAI_OBJECT_TYPE_ACL_ENTRY);
    map[SAI_OBJECT_TYPE_ACL_COUNTER] = TO_STR(SAI_OBJECT_TYPE_ACL_COUNTER);
    map[SAI_OBJECT_TYPE_HOST_INTERFACE] = TO_STR(SAI_OBJECT_TYPE_HOST_INTERFACE);
    map[SAI_OBJECT_TYPE_MIRROR] = TO_STR(SAI_OBJECT_TYPE_MIRROR);
    map[SAI_OBJECT_TYPE_SAMPLEPACKET] = TO_STR(SAI_OBJECT_TYPE_SAMPLEPACKET);
    map[SAI_OBJECT_TYPE_STP_INSTANCE] = TO_STR(SAI_OBJECT_TYPE_STP_INSTANCE);
    map[SAI_OBJECT_TYPE_TRAP_GROUP] = TO_STR(SAI_OBJECT_TYPE_TRAP_GROUP);
    map[SAI_OBJECT_TYPE_ACL_TABLE_GROUP] = TO_STR(SAI_OBJECT_TYPE_ACL_TABLE_GROUP);
    map[SAI_OBJECT_TYPE_POLICER] = TO_STR(SAI_OBJECT_TYPE_POLICER);
    map[SAI_OBJECT_TYPE_WRED] = TO_STR(SAI_OBJECT_TYPE_WRED);
    map[SAI_OBJECT_TYPE_QOS_MAPS] = TO_STR(SAI_OBJECT_TYPE_QOS_MAPS);
    map[SAI_OBJECT_TYPE_QUEUE] = TO_STR(SAI_OBJECT_TYPE_QUEUE);
    map[SAI_OBJECT_TYPE_SCHEDULER] = TO_STR(SAI_OBJECT_TYPE_SCHEDULER);
    map[SAI_OBJECT_TYPE_SCHEDULER_GROUP] = TO_STR(SAI_OBJECT_TYPE_SCHEDULER_GROUP);
    map[SAI_OBJECT_TYPE_BUFFER_POOL] = TO_STR(SAI_OBJECT_TYPE_BUFFER_POOL);
    map[SAI_OBJECT_TYPE_BUFFER_PROFILE] = TO_STR(SAI_OBJECT_TYPE_BUFFER_PROFILE);
    map[SAI_OBJECT_TYPE_PRIORITY_GROUP] = TO_STR(SAI_OBJECT_TYPE_PRIORITY_GROUP);
    map[SAI_OBJECT_TYPE_LAG_MEMBER] = TO_STR(SAI_OBJECT_TYPE_LAG_MEMBER);
    map[SAI_OBJECT_TYPE_HASH] = TO_STR(SAI_OBJECT_TYPE_HASH);
    map[SAI_OBJECT_TYPE_UDF] = TO_STR(SAI_OBJECT_TYPE_UDF);
    map[SAI_OBJECT_TYPE_UDF_MATCH] = TO_STR(SAI_OBJECT_TYPE_UDF_MATCH);
    map[SAI_OBJECT_TYPE_UDF_GROUP] = TO_STR(SAI_OBJECT_TYPE_UDF_GROUP);
    map[SAI_OBJECT_TYPE_FDB] = TO_STR(SAI_OBJECT_TYPE_FDB);
    map[SAI_OBJECT_TYPE_SWITCH] = TO_STR(SAI_OBJECT_TYPE_SWITCH);
    map[SAI_OBJECT_TYPE_TRAP] = TO_STR(SAI_OBJECT_TYPE_TRAP);
    map[SAI_OBJECT_TYPE_TRAP_USER_DEF] = TO_STR(SAI_OBJECT_TYPE_TRAP_USER_DEF);
    map[SAI_OBJECT_TYPE_NEIGHBOR] = TO_STR(SAI_OBJECT_TYPE_NEIGHBOR);
    map[SAI_OBJECT_TYPE_ROUTE] = TO_STR(SAI_OBJECT_TYPE_ROUTE);
    map[SAI_OBJECT_TYPE_VLAN] = TO_STR(SAI_OBJECT_TYPE_VLAN);
    map[SAI_OBJECT_TYPE_TUNNEL] = TO_STR(SAI_OBJECT_TYPE_TUNNEL);
    map[SAI_OBJECT_TYPE_TUNNEL_TABLE_ENTRY] = TO_STR(SAI_OBJECT_TYPE_TUNNEL_TABLE_ENTRY);

    return map;
}

sai_status_t sai_get_object_type_string(sai_object_type_t object_type, std::string &str_object_type)
{
    auto it = g_object_type_map.find(object_type);

    if (it == g_object_type_map.end())
    {
        SWSS_LOG_ERROR("serialization object not found %x", object_type);

        return SAI_STATUS_NOT_IMPLEMENTED;
    }

    str_object_type = it->second;

    return SAI_STATUS_SUCCESS;
}

static std::vector<std::string> sai_get_port_stat_counter_names()
{
    SWSS_LOG_ENTER();

    std::vector<std::string> v;

    v.resize(SAI_PORT_STAT_ETHER_OUT_PKTS_9217_TO_16383_OCTETS + 1);

    #define ENTRY(x) v[x] = std::string( #x )

    ENTRY(SAI_PORT_STAT_IF_IN_OCTETS);
    ENTRY(SAI_PORT_STAT_IF_IN_UCAST_PKTS);
    ENTRY(SAI_PORT_STAT_IF_IN_NON_UCAST_PKTS);
    ENTRY(SAI_PORT_STAT_IF_IN_DISCARDS);
    ENTRY(SAI_PORT_STAT_IF_IN_ERRORS);
    ENTRY(SAI_PORT_STAT_IF_IN_UNKNOWN_PROTOS);
    ENTRY(SAI_PORT_STAT_IF_IN_BROADCAST_PKTS);
    ENTRY(SAI_PORT_STAT_IF_IN_MULTICAST_PKTS);
    ENTRY(SAI_PORT_STAT_IF_IN_VLAN_DISCARDS);
    ENTRY(SAI_PORT_STAT_IF_OUT_OCTETS);
    ENTRY(SAI_PORT_STAT_IF_OUT_UCAST_PKTS);
    ENTRY(SAI_PORT_STAT_IF_OUT_NON_UCAST_PKTS);
    ENTRY(SAI_PORT_STAT_IF_OUT_DISCARDS);
    ENTRY(SAI_PORT_STAT_IF_OUT_ERRORS);
    ENTRY(SAI_PORT_STAT_IF_OUT_QLEN);
    ENTRY(SAI_PORT_STAT_IF_OUT_BROADCAST_PKTS);
    ENTRY(SAI_PORT_STAT_IF_OUT_MULTICAST_PKTS);
    ENTRY(SAI_PORT_STAT_ETHER_STATS_DROP_EVENTS);
    ENTRY(SAI_PORT_STAT_ETHER_STATS_MULTICAST_PKTS);
    ENTRY(SAI_PORT_STAT_ETHER_STATS_BROADCAST_PKTS);
    ENTRY(SAI_PORT_STAT_ETHER_STATS_UNDERSIZE_PKTS);
    ENTRY(SAI_PORT_STAT_ETHER_STATS_FRAGMENTS);
    ENTRY(SAI_PORT_STAT_ETHER_STATS_PKTS_64_OCTETS);
    ENTRY(SAI_PORT_STAT_ETHER_STATS_PKTS_65_TO_127_OCTETS);
    ENTRY(SAI_PORT_STAT_ETHER_STATS_PKTS_128_TO_255_OCTETS);
    ENTRY(SAI_PORT_STAT_ETHER_STATS_PKTS_256_TO_511_OCTETS);
    ENTRY(SAI_PORT_STAT_ETHER_STATS_PKTS_512_TO_1023_OCTETS);
    ENTRY(SAI_PORT_STAT_ETHER_STATS_PKTS_1024_TO_1518_OCTETS);
    ENTRY(SAI_PORT_STAT_ETHER_STATS_PKTS_1519_TO_2047_OCTETS);
    ENTRY(SAI_PORT_STAT_ETHER_STATS_PKTS_2048_TO_4095_OCTETS);
    ENTRY(SAI_PORT_STAT_ETHER_STATS_PKTS_4096_TO_9216_OCTETS);
    ENTRY(SAI_PORT_STAT_ETHER_STATS_PKTS_9217_TO_16383_OCTETS);
    ENTRY(SAI_PORT_STAT_ETHER_STATS_OVERSIZE_PKTS);
    ENTRY(SAI_PORT_STAT_ETHER_RX_OVERSIZE_PKTS);
    ENTRY(SAI_PORT_STAT_ETHER_TX_OVERSIZE_PKTS);
    ENTRY(SAI_PORT_STAT_ETHER_STATS_JABBERS);
    ENTRY(SAI_PORT_STAT_ETHER_STATS_OCTETS);
    ENTRY(SAI_PORT_STAT_ETHER_STATS_PKTS);
    ENTRY(SAI_PORT_STAT_ETHER_STATS_COLLISIONS);
    ENTRY(SAI_PORT_STAT_ETHER_STATS_CRC_ALIGN_ERRORS);
    ENTRY(SAI_PORT_STAT_ETHER_STATS_TX_NO_ERRORS);
    ENTRY(SAI_PORT_STAT_ETHER_STATS_RX_NO_ERRORS);
    ENTRY(SAI_PORT_STAT_IP_IN_RECEIVES);
    ENTRY(SAI_PORT_STAT_IP_IN_OCTETS);
    ENTRY(SAI_PORT_STAT_IP_IN_UCAST_PKTS);
    ENTRY(SAI_PORT_STAT_IP_IN_NON_UCAST_PKTS);
    ENTRY(SAI_PORT_STAT_IP_IN_DISCARDS);
    ENTRY(SAI_PORT_STAT_IP_OUT_OCTETS);
    ENTRY(SAI_PORT_STAT_IP_OUT_UCAST_PKTS);
    ENTRY(SAI_PORT_STAT_IP_OUT_NON_UCAST_PKTS);
    ENTRY(SAI_PORT_STAT_IP_OUT_DISCARDS);
    ENTRY(SAI_PORT_STAT_IPV6_IN_RECEIVES);
    ENTRY(SAI_PORT_STAT_IPV6_IN_OCTETS);
    ENTRY(SAI_PORT_STAT_IPV6_IN_UCAST_PKTS);
    ENTRY(SAI_PORT_STAT_IPV6_IN_NON_UCAST_PKTS);
    ENTRY(SAI_PORT_STAT_IPV6_IN_MCAST_PKTS);
    ENTRY(SAI_PORT_STAT_IPV6_IN_DISCARDS);
    ENTRY(SAI_PORT_STAT_IPV6_OUT_OCTETS);
    ENTRY(SAI_PORT_STAT_IPV6_OUT_UCAST_PKTS);
    ENTRY(SAI_PORT_STAT_IPV6_OUT_NON_UCAST_PKTS);
    ENTRY(SAI_PORT_STAT_IPV6_OUT_MCAST_PKTS);
    ENTRY(SAI_PORT_STAT_IPV6_OUT_DISCARDS);
    ENTRY(SAI_PORT_STAT_GREEN_DISCARD_DROPPED_PACKETS);
    ENTRY(SAI_PORT_STAT_GREEN_DISCARD_DROPPED_BYTES);
    ENTRY(SAI_PORT_STAT_YELLOW_DISCARD_DROPPED_PACKETS);
    ENTRY(SAI_PORT_STAT_YELLOW_DISCARD_DROPPED_BYTES);
    ENTRY(SAI_PORT_STAT_RED_DISCARD_DROPPED_PACKETS);
    ENTRY(SAI_PORT_STAT_RED_DISCARD_DROPPED_BYTES);
    ENTRY(SAI_PORT_STAT_DISCARD_DROPPED_PACKETS);
    ENTRY(SAI_PORT_STAT_DISCARD_DROPPED_BYTES);
    ENTRY(SAI_PORT_STAT_ETHER_IN_PKTS_64_OCTETS);
    ENTRY(SAI_PORT_STAT_ETHER_IN_PKTS_65_TO_127_OCTETS);
    ENTRY(SAI_PORT_STAT_ETHER_IN_PKTS_128_TO_255_OCTETS);
    ENTRY(SAI_PORT_STAT_ETHER_IN_PKTS_256_TO_511_OCTETS);
    ENTRY(SAI_PORT_STAT_ETHER_IN_PKTS_512_TO_1023_OCTETS);
    ENTRY(SAI_PORT_STAT_ETHER_IN_PKTS_1024_TO_1518_OCTETS);
    ENTRY(SAI_PORT_STAT_ETHER_IN_PKTS_1519_TO_2047_OCTETS);
    ENTRY(SAI_PORT_STAT_ETHER_IN_PKTS_2048_TO_4095_OCTETS);
    ENTRY(SAI_PORT_STAT_ETHER_IN_PKTS_4096_TO_9216_OCTETS);
    ENTRY(SAI_PORT_STAT_ETHER_IN_PKTS_9217_TO_16383_OCTETS);
    ENTRY(SAI_PORT_STAT_ETHER_OUT_PKTS_64_OCTETS);
    ENTRY(SAI_PORT_STAT_ETHER_OUT_PKTS_65_TO_127_OCTETS);
    ENTRY(SAI_PORT_STAT_ETHER_OUT_PKTS_128_TO_255_OCTETS);
    ENTRY(SAI_PORT_STAT_ETHER_OUT_PKTS_256_TO_511_OCTETS);
    ENTRY(SAI_PORT_STAT_ETHER_OUT_PKTS_512_TO_1023_OCTETS);
    ENTRY(SAI_PORT_STAT_ETHER_OUT_PKTS_1024_TO_1518_OCTETS);
    ENTRY(SAI_PORT_STAT_ETHER_OUT_PKTS_1519_TO_2047_OCTETS);
    ENTRY(SAI_PORT_STAT_ETHER_OUT_PKTS_2048_TO_4095_OCTETS);
    ENTRY(SAI_PORT_STAT_ETHER_OUT_PKTS_4096_TO_9216_OCTETS);
    ENTRY(SAI_PORT_STAT_ETHER_OUT_PKTS_9217_TO_16383_OCTETS);

    return std::move(v);
}

static std::vector<std::string> g_port_stat_counter_names = sai_get_port_stat_counter_names();

std::string sai_get_port_stat_counter_name(sai_port_stat_counter_t counter)
{
    if (counter < g_port_stat_counter_names.size())
        return g_port_stat_counter_names[counter];

    SWSS_LOG_WARN("unable to find counter name: %d", counter);

    // TODO exit here ?
    return std::to_string(counter);
}

sai_status_t sai_get_serialization_type(
        _In_ const sai_object_type_t object_type,
        _In_ const sai_attr_id_t attr_id,
        _Out_ sai_attr_serialization_type_t &serialization_type)
{
    auto it = g_serialization_map.find(object_type);

    if (it == g_serialization_map.end())
    {
        SWSS_LOG_ERROR("serialization object not found %x", object_type);

        return SAI_STATUS_NOT_IMPLEMENTED;
    }

    std::map<sai_attr_id_t, sai_attr_serialization_type_t> &map = it->second;

    auto mit = map.find(attr_id);

    if (mit == map.end())
    {
        SWSS_LOG_ERROR("serialization attribute id not found %u for object type : %u", attr_id, object_type);

        return SAI_STATUS_NOT_IMPLEMENTED;
    }

    serialization_type = mit->second;

    return SAI_STATUS_SUCCESS;
}

void sai_deserialize_buffer(
        _In_ const std::string &s,
        _In_ int index,
        _In_ size_t buffer_size,
        _In_ void *buffer)
{
    unsigned char *mem = reinterpret_cast<unsigned char*>(buffer);

    const char *ptr = s.c_str() + index;

    for (size_t i = 0; i < buffer_size; i ++)
    {
        int u = char_to_int(ptr[2 * i]);
        int l = char_to_int(ptr[2 * i + 1]);

        int c = (u << 4) | l;

        mem[i] = (unsigned char) c;
    }

    index += (int)(buffer_size * 2);
}

void sai_free_buffer(void *buffer)
{
    delete (unsigned char*) buffer;
}

void sai_serialize_buffer(
        _In_ const void *buffer,
        _In_ size_t buffer_size,
        _Out_ std::string &s)
{
    std::stringstream ss;

    unsigned const char* mem = reinterpret_cast<const unsigned char*>(buffer);

    for (size_t i = 0; i < buffer_size; i++)
    {
        ss << std::setfill('0') << std::setw(2) << std::hex << (unsigned int)mem[i];
    }

    s += ss.str();
}

sai_status_t sai_serialize_attr_id(
        _In_ const sai_attribute_t &attr,
        _Out_ std::string &s)
{
    sai_serialize_primitive(attr.id, s);

    return SAI_STATUS_SUCCESS;
}

sai_status_t sai_serialize_attr_value(
        _In_ const sai_attr_serialization_type_t type,
        _In_ const sai_attribute_t &attr,
        _Out_ std::string &s,
        _In_ bool countOnly)
{
    switch (type)
    {
        case SAI_SERIALIZATION_TYPE_BOOL:
            sai_serialize_primitive(attr.value.booldata, s);
            break;

        case SAI_SERIALIZATION_TYPE_CHARDATA:
            sai_serialize_primitive(attr.value.chardata, s);
            break;

        case SAI_SERIALIZATION_TYPE_UINT8:
            sai_serialize_primitive(attr.value.u8, s);
            break;

        case SAI_SERIALIZATION_TYPE_INT8:
            sai_serialize_primitive(attr.value.s8, s);
            break;

        case SAI_SERIALIZATION_TYPE_UINT16:
            sai_serialize_primitive(attr.value.u16, s);
            break;

        case SAI_SERIALIZATION_TYPE_INT16:
            sai_serialize_primitive(attr.value.s16, s);
            break;

        case SAI_SERIALIZATION_TYPE_UINT32:
            sai_serialize_primitive(attr.value.u32, s);
            break;

        case SAI_SERIALIZATION_TYPE_INT32:
            sai_serialize_primitive(attr.value.s32, s);
            break;

        case SAI_SERIALIZATION_TYPE_UINT64:
            sai_serialize_primitive(attr.value.u64, s);
            break;

        case SAI_SERIALIZATION_TYPE_INT64:
            sai_serialize_primitive(attr.value.s64, s);
            break;

        case SAI_SERIALIZATION_TYPE_MAC:
            sai_serialize_primitive(attr.value.mac, s);
            break;

        case SAI_SERIALIZATION_TYPE_IP4:
            sai_serialize_primitive(attr.value.ip4, s);
            break;

        case SAI_SERIALIZATION_TYPE_IP6:
            sai_serialize_primitive(attr.value.ip6, s);
            break;

        case SAI_SERIALIZATION_TYPE_IP_ADDRESS:
            sai_serialize_ip_address(attr.value.ipaddr, s);
            break;

        case SAI_SERIALIZATION_TYPE_OBJECT_ID:
            sai_serialize_primitive(attr.value.oid, s);
            break;

        case SAI_SERIALIZATION_TYPE_OBJECT_LIST:
            sai_serialize_list(attr.value.objlist, s, countOnly);
            break;

        case SAI_SERIALIZATION_TYPE_UINT8_LIST:
            sai_serialize_list(attr.value.u8list, s, countOnly);
            break;

        case SAI_SERIALIZATION_TYPE_INT8_LIST:
            sai_serialize_list(attr.value.s8list, s, countOnly);
            break;

        case SAI_SERIALIZATION_TYPE_UINT16_LIST:
            sai_serialize_list(attr.value.u16list, s, countOnly);
            break;

        case SAI_SERIALIZATION_TYPE_INT16_LIST:
            sai_serialize_list(attr.value.s16list, s, countOnly);
            break;

        case SAI_SERIALIZATION_TYPE_UINT32_LIST:
            sai_serialize_list(attr.value.u32list, s, countOnly);
            break;

        case SAI_SERIALIZATION_TYPE_INT32_LIST:
            sai_serialize_list(attr.value.s32list, s, countOnly);
            break;

        case SAI_SERIALIZATION_TYPE_UINT32_RANGE:
            sai_serialize_primitive(attr.value.u32range, s);
            break;

        case SAI_SERIALIZATION_TYPE_INT32_RANGE:
            sai_serialize_primitive(attr.value.s32range, s);
            break;

        case SAI_SERIALIZATION_TYPE_VLAN_LIST:
            sai_serialize_list(attr.value.vlanlist, s, countOnly);
            break;

        case SAI_SERIALIZATION_TYPE_PORT_BREAKOUT:
            sai_serialize_primitive(attr.value.portbreakout.breakout_mode, s);
            sai_serialize_list(attr.value.portbreakout.port_list, s, countOnly);
            break;

        case SAI_SERIALIZATION_TYPE_QOS_MAP_LIST:
            sai_serialize_list(attr.value.qosmap, s, countOnly);
            break;

        case SAI_SERIALIZATION_TYPE_TUNNEL_MAP_LIST:
            sai_serialize_list(attr.value.tunnelmap, s, countOnly);
            break;

            /* ACL FIELD DATA */

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_BOOL:
            sai_serialize_primitive(attr.value.aclfield.enable, s);
            sai_serialize_primitive(attr.value.aclfield.data.booldata, s);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT8:
            sai_serialize_primitive(attr.value.aclfield.enable, s);
            sai_serialize_primitive(attr.value.aclfield.mask.u8, s);
            sai_serialize_primitive(attr.value.aclfield.data.u8, s);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_INT8:
            sai_serialize_primitive(attr.value.aclfield.enable, s);
            sai_serialize_primitive(attr.value.aclfield.mask.s8, s);
            sai_serialize_primitive(attr.value.aclfield.data.s8, s);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT16:
            sai_serialize_primitive(attr.value.aclfield.enable, s);
            sai_serialize_primitive(attr.value.aclfield.mask.u16, s);
            sai_serialize_primitive(attr.value.aclfield.data.u16, s);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_INT16:
            sai_serialize_primitive(attr.value.aclfield.enable, s);
            sai_serialize_primitive(attr.value.aclfield.mask.s16, s);
            sai_serialize_primitive(attr.value.aclfield.data.s16, s);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT32:
            sai_serialize_primitive(attr.value.aclfield.enable, s);
            sai_serialize_primitive(attr.value.aclfield.mask.u16, s);
            sai_serialize_primitive(attr.value.aclfield.data.u16, s);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_INT32:
            sai_serialize_primitive(attr.value.aclfield.enable, s);
            sai_serialize_primitive(attr.value.aclfield.mask.s32, s);
            sai_serialize_primitive(attr.value.aclfield.data.s32, s);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_MAC:
            sai_serialize_primitive(attr.value.aclfield.enable, s);
            sai_serialize_primitive(attr.value.aclfield.mask.mac, s);
            sai_serialize_primitive(attr.value.aclfield.data.mac, s);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_IP4:
            sai_serialize_primitive(attr.value.aclfield.enable, s);
            sai_serialize_primitive(attr.value.aclfield.mask.ip4, s);
            sai_serialize_primitive(attr.value.aclfield.data.ip4, s);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_IP6:
            sai_serialize_primitive(attr.value.aclfield.enable, s);
            sai_serialize_primitive(attr.value.aclfield.mask.ip6, s);
            sai_serialize_primitive(attr.value.aclfield.data.ip6, s);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_OBJECT_ID:
            sai_serialize_primitive(attr.value.aclfield.enable, s);
            sai_serialize_primitive(attr.value.aclfield.data.oid, s);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_OBJECT_LIST:
            sai_serialize_primitive(attr.value.aclfield.enable, s);
            sai_serialize_list(attr.value.aclfield.data.objlist, s, countOnly);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT8_LIST:
            sai_serialize_primitive(attr.value.aclfield.enable, s);
            sai_serialize_list(attr.value.aclfield.mask.u8list, s, countOnly);
            sai_serialize_list(attr.value.aclfield.data.u8list, s, countOnly);
            break;

            /* ACL ACTION DATA */

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_UINT8:
            sai_serialize_primitive(attr.value.aclaction.enable, s);
            sai_serialize_primitive(attr.value.aclaction.parameter.u8, s);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_INT8:
            sai_serialize_primitive(attr.value.aclaction.enable, s);
            sai_serialize_primitive(attr.value.aclaction.parameter.s8, s);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_UINT16:
            sai_serialize_primitive(attr.value.aclaction.enable, s);
            sai_serialize_primitive(attr.value.aclaction.parameter.u16, s);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_INT16:
            sai_serialize_primitive(attr.value.aclaction.enable, s);
            sai_serialize_primitive(attr.value.aclaction.parameter.s16, s);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_UINT32:
            sai_serialize_primitive(attr.value.aclaction.enable, s);
            sai_serialize_primitive(attr.value.aclaction.parameter.u32, s);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_INT32:
            sai_serialize_primitive(attr.value.aclaction.enable, s);
            sai_serialize_primitive(attr.value.aclaction.parameter.s32, s);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_MAC:
            sai_serialize_primitive(attr.value.aclaction.enable, s);
            sai_serialize_primitive(attr.value.aclaction.parameter.mac, s);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_IPV4:
            sai_serialize_primitive(attr.value.aclaction.enable, s);
            sai_serialize_primitive(attr.value.aclaction.parameter.ip4, s);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_IPV6:
            sai_serialize_primitive(attr.value.aclaction.enable, s);
            sai_serialize_primitive(attr.value.aclaction.parameter.ip6, s);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_ID:
            sai_serialize_primitive(attr.value.aclaction.enable, s);
            sai_serialize_primitive(attr.value.aclaction.parameter.oid, s);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_LIST:
            sai_serialize_primitive(attr.value.aclaction.enable, s);
            sai_serialize_list(attr.value.aclaction.parameter.objlist, s, countOnly);
            break;

        default:
            return SAI_STATUS_NOT_IMPLEMENTED;
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t sai_serialize_attr(
        _In_ const sai_attr_serialization_type_t type,
        _In_ const sai_attribute_t &attr,
        _Out_ std::string &s,
        _In_ bool countOnly)
{
    sai_serialize_attr_id(attr, s);

    return sai_serialize_attr_value(type, attr, s, countOnly);
}

int char_to_int(
        _In_ const char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';

    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;

    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;

    SWSS_LOG_ERROR("Unable to convert char %d to int", c);

    exit(EXIT_FAILURE);
}

sai_status_t sai_deserialize_attr_value(
        _In_ const std::string &s,
        _In_ int &index,
        _In_ const sai_attr_serialization_type_t type,
        _Out_ sai_attribute_t &attr,
        _In_ bool countOnly)
{
    switch (type)
    {
        case SAI_SERIALIZATION_TYPE_BOOL:
            sai_deserialize_primitive(s, index, attr.value.booldata);
            break;

        case SAI_SERIALIZATION_TYPE_CHARDATA:
            sai_deserialize_primitive(s, index, attr.value.chardata);
            break;

        case SAI_SERIALIZATION_TYPE_UINT8:
            sai_deserialize_primitive(s, index, attr.value.u8);
            break;

        case SAI_SERIALIZATION_TYPE_INT8:
            sai_deserialize_primitive(s, index, attr.value.s8);
            break;

        case SAI_SERIALIZATION_TYPE_UINT16:
            sai_deserialize_primitive(s, index, attr.value.u16);
            break;

        case SAI_SERIALIZATION_TYPE_INT16:
            sai_deserialize_primitive(s, index, attr.value.s16);
            break;

        case SAI_SERIALIZATION_TYPE_UINT32:
            sai_deserialize_primitive(s, index, attr.value.u32);
            break;

        case SAI_SERIALIZATION_TYPE_INT32:
            sai_deserialize_primitive(s, index, attr.value.s32);
            break;

        case SAI_SERIALIZATION_TYPE_UINT64:
            sai_deserialize_primitive(s, index, attr.value.u64);
            break;

        case SAI_SERIALIZATION_TYPE_INT64:
            sai_deserialize_primitive(s, index, attr.value.s64);
            break;

        case SAI_SERIALIZATION_TYPE_MAC:
            sai_deserialize_primitive(s, index, attr.value.mac);
            break;

        case SAI_SERIALIZATION_TYPE_IP4:
            sai_deserialize_primitive(s, index, attr.value.ip4);
            break;

        case SAI_SERIALIZATION_TYPE_IP6:
            sai_deserialize_primitive(s, index, attr.value.ip6);
            break;

        case SAI_SERIALIZATION_TYPE_IP_ADDRESS:
            sai_deserialize_ip_address(s, index, attr.value.ipaddr);
            break;

        case SAI_SERIALIZATION_TYPE_OBJECT_ID:
            sai_deserialize_primitive(s, index, attr.value.oid);
            break;

        case SAI_SERIALIZATION_TYPE_OBJECT_LIST:
            sai_deserialize_list(s, index, attr.value.objlist, countOnly);
            break;

        case SAI_SERIALIZATION_TYPE_UINT8_LIST:
            sai_deserialize_list(s, index, attr.value.u8list, countOnly);
            break;

        case SAI_SERIALIZATION_TYPE_INT8_LIST:
            sai_deserialize_list(s, index, attr.value.s8list, countOnly);
            break;

        case SAI_SERIALIZATION_TYPE_UINT16_LIST:
            sai_deserialize_list(s, index, attr.value.u16list, countOnly);
            break;

        case SAI_SERIALIZATION_TYPE_INT16_LIST:
            sai_deserialize_list(s, index, attr.value.s16list, countOnly);
            break;

        case SAI_SERIALIZATION_TYPE_UINT32_LIST:
            sai_deserialize_list(s, index, attr.value.u32list, countOnly);
            break;

        case SAI_SERIALIZATION_TYPE_INT32_LIST:
            sai_deserialize_list(s, index, attr.value.s32list, countOnly);
            break;

        case SAI_SERIALIZATION_TYPE_UINT32_RANGE:
            sai_deserialize_primitive(s, index, attr.value.u32range);
            break;

        case SAI_SERIALIZATION_TYPE_INT32_RANGE:
            sai_deserialize_primitive(s, index, attr.value.s32range);
            break;

        case SAI_SERIALIZATION_TYPE_VLAN_LIST:
            sai_deserialize_list(s, index, attr.value.vlanlist, countOnly);
            break;

        case SAI_SERIALIZATION_TYPE_PORT_BREAKOUT:
            sai_deserialize_primitive(s, index, attr.value.portbreakout.breakout_mode);
            sai_deserialize_list(s, index, attr.value.portbreakout.port_list, countOnly);
            break;

        case SAI_SERIALIZATION_TYPE_QOS_MAP_LIST:
            sai_deserialize_list(s, index, attr.value.qosmap, countOnly);
            break;

        case SAI_SERIALIZATION_TYPE_TUNNEL_MAP_LIST:
            sai_deserialize_list(s, index, attr.value.tunnelmap, countOnly);
            break;

            /* ACL FIELD DATA */

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_BOOL:
            sai_deserialize_primitive(s, index, attr.value.aclfield.enable);
            sai_deserialize_primitive(s, index, attr.value.aclfield.data.booldata);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT8:
            sai_deserialize_primitive(s, index, attr.value.aclfield.enable);
            sai_deserialize_primitive(s, index, attr.value.aclfield.mask.u8);
            sai_deserialize_primitive(s, index, attr.value.aclfield.data.u8);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_INT8:
            sai_deserialize_primitive(s, index, attr.value.aclfield.enable);
            sai_deserialize_primitive(s, index, attr.value.aclfield.mask.s8);
            sai_deserialize_primitive(s, index, attr.value.aclfield.data.s8);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT16:
            sai_deserialize_primitive(s, index, attr.value.aclfield.enable);
            sai_deserialize_primitive(s, index, attr.value.aclfield.mask.u16);
            sai_deserialize_primitive(s, index, attr.value.aclfield.data.u16);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_INT16:
            sai_deserialize_primitive(s, index, attr.value.aclfield.enable);
            sai_deserialize_primitive(s, index, attr.value.aclfield.mask.s16);
            sai_deserialize_primitive(s, index, attr.value.aclfield.data.s16);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT32:
            sai_deserialize_primitive(s, index, attr.value.aclfield.enable);
            sai_deserialize_primitive(s, index, attr.value.aclfield.mask.u16);
            sai_deserialize_primitive(s, index, attr.value.aclfield.data.u16);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_INT32:
            sai_deserialize_primitive(s, index, attr.value.aclfield.enable);
            sai_deserialize_primitive(s, index, attr.value.aclfield.mask.s32);
            sai_deserialize_primitive(s, index, attr.value.aclfield.data.s32);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_MAC:
            sai_deserialize_primitive(s, index, attr.value.aclfield.enable);
            sai_deserialize_primitive(s, index, attr.value.aclfield.mask.mac);
            sai_deserialize_primitive(s, index, attr.value.aclfield.data.mac);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_IP4:
            sai_deserialize_primitive(s, index, attr.value.aclfield.enable);
            sai_deserialize_primitive(s, index, attr.value.aclfield.mask.ip4);
            sai_deserialize_primitive(s, index, attr.value.aclfield.data.ip4);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_IP6:
            sai_deserialize_primitive(s, index, attr.value.aclfield.enable);
            sai_deserialize_primitive(s, index, attr.value.aclfield.mask.ip6);
            sai_deserialize_primitive(s, index, attr.value.aclfield.data.ip6);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_OBJECT_ID:
            sai_deserialize_primitive(s, index, attr.value.aclfield.enable);
            sai_deserialize_primitive(s, index, attr.value.aclfield.data.oid);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_OBJECT_LIST:
            sai_deserialize_primitive(s, index, attr.value.aclfield.enable);
            sai_deserialize_list(s, index, attr.value.aclfield.data.objlist, countOnly);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT8_LIST:
            sai_deserialize_primitive(s, index, attr.value.aclfield.enable);
            sai_deserialize_list(s, index, attr.value.aclfield.mask.u8list, countOnly);
            sai_deserialize_list(s, index, attr.value.aclfield.data.u8list, countOnly);
            break;

            /* ACL ACTION DATA */

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_UINT8:
            sai_deserialize_primitive(s, index, attr.value.aclaction.enable);
            sai_deserialize_primitive(s, index, attr.value.aclaction.parameter.u8);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_INT8:
            sai_deserialize_primitive(s, index, attr.value.aclaction.enable);
            sai_deserialize_primitive(s, index, attr.value.aclaction.parameter.s8);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_UINT16:
            sai_deserialize_primitive(s, index, attr.value.aclaction.enable);
            sai_deserialize_primitive(s, index, attr.value.aclaction.parameter.u16);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_INT16:
            sai_deserialize_primitive(s, index, attr.value.aclaction.enable);
            sai_deserialize_primitive(s, index, attr.value.aclaction.parameter.s16);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_UINT32:
            sai_deserialize_primitive(s, index, attr.value.aclaction.enable);
            sai_deserialize_primitive(s, index, attr.value.aclaction.parameter.u32);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_INT32:
            sai_deserialize_primitive(s, index, attr.value.aclaction.enable);
            sai_deserialize_primitive(s, index, attr.value.aclaction.parameter.s32);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_MAC:
            sai_deserialize_primitive(s, index, attr.value.aclaction.enable);
            sai_deserialize_primitive(s, index, attr.value.aclaction.parameter.mac);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_IPV4:
            sai_deserialize_primitive(s, index, attr.value.aclaction.enable);
            sai_deserialize_primitive(s, index, attr.value.aclaction.parameter.ip4);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_IPV6:
            sai_deserialize_primitive(s, index, attr.value.aclaction.enable);
            sai_deserialize_primitive(s, index, attr.value.aclaction.parameter.ip6);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_ID:
            sai_deserialize_primitive(s, index, attr.value.aclaction.enable);
            sai_deserialize_primitive(s, index, attr.value.aclaction.parameter.oid);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_LIST:
            sai_deserialize_primitive(s, index, attr.value.aclaction.enable);
            sai_deserialize_list(s, index, attr.value.aclaction.parameter.objlist, countOnly);
            break;

        default:
            return SAI_STATUS_NOT_IMPLEMENTED;
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t sai_deserialize_free_attribute_value(
        _In_ const sai_attr_serialization_type_t type,
        _In_ sai_attribute_t &attr)
{
    // if we allocated list, then we need to free it

    switch (type)
    {
        case SAI_SERIALIZATION_TYPE_BOOL:
            break;

        case SAI_SERIALIZATION_TYPE_CHARDATA:
            break;

        case SAI_SERIALIZATION_TYPE_UINT8:
            break;

        case SAI_SERIALIZATION_TYPE_INT8:
            break;

        case SAI_SERIALIZATION_TYPE_UINT16:
            break;

        case SAI_SERIALIZATION_TYPE_INT16:
            break;

        case SAI_SERIALIZATION_TYPE_UINT32:
            break;

        case SAI_SERIALIZATION_TYPE_INT32:
            break;

        case SAI_SERIALIZATION_TYPE_UINT64:
            break;

        case SAI_SERIALIZATION_TYPE_INT64:
            break;

        case SAI_SERIALIZATION_TYPE_MAC:
            break;

        case SAI_SERIALIZATION_TYPE_IP4:
            break;

        case SAI_SERIALIZATION_TYPE_IP6:
            break;

        case SAI_SERIALIZATION_TYPE_IP_ADDRESS:
            break;

        case SAI_SERIALIZATION_TYPE_OBJECT_ID:
            break;

        case SAI_SERIALIZATION_TYPE_OBJECT_LIST:
            sai_free_list(attr.value.objlist);
            break;

        case SAI_SERIALIZATION_TYPE_UINT8_LIST:
            sai_free_list(attr.value.u8list);
            break;

        case SAI_SERIALIZATION_TYPE_INT8_LIST:
            sai_free_list(attr.value.s8list);
            break;

        case SAI_SERIALIZATION_TYPE_UINT16_LIST:
            sai_free_list(attr.value.u16list);
            break;

        case SAI_SERIALIZATION_TYPE_INT16_LIST:
            sai_free_list(attr.value.s16list);
            break;

        case SAI_SERIALIZATION_TYPE_UINT32_LIST:
            sai_free_list(attr.value.u32list);
            break;

        case SAI_SERIALIZATION_TYPE_INT32_LIST:
            sai_free_list(attr.value.s32list);
            break;

        case SAI_SERIALIZATION_TYPE_UINT32_RANGE:
            break;

        case SAI_SERIALIZATION_TYPE_INT32_RANGE:
            break;

        case SAI_SERIALIZATION_TYPE_VLAN_LIST:
            sai_free_list(attr.value.vlanlist);
            break;

        case SAI_SERIALIZATION_TYPE_PORT_BREAKOUT:
            sai_free_list(attr.value.portbreakout.port_list);
            break;

        case SAI_SERIALIZATION_TYPE_QOS_MAP_LIST:
            sai_free_list(attr.value.qosmap);
            break;

        case SAI_SERIALIZATION_TYPE_TUNNEL_MAP_LIST:
            sai_free_list(attr.value.tunnelmap);
            break;

            /* ACL FIELD DATA */

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_BOOL:
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT8:
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_INT8:
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT16:
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_INT16:
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT32:
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_INT32:
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_MAC:
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_IP4:
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_IP6:
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_OBJECT_ID:
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_OBJECT_LIST:
            sai_free_list(attr.value.aclfield.data.objlist);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT8_LIST:
            sai_free_list(attr.value.aclfield.mask.u8list);
            sai_free_list(attr.value.aclfield.data.u8list);
            break;

            /* ACL ACTION DATA */

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_UINT8:
            break;

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_INT8:
            break;

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_UINT16:
            break;

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_INT16:
            break;

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_UINT32:
            break;

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_INT32:
            break;

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_MAC:
            break;

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_IPV4:
            break;

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_IPV6:
            break;

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_ID:
            break;

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_LIST:
            sai_free_list(attr.value.aclaction.parameter.objlist);
            break;

        default:
            return SAI_STATUS_NOT_IMPLEMENTED;
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t sai_serialize_fdb_event_notification_data(
        _In_ sai_fdb_event_notification_data_t *fdb,
        _Out_ std::string &s)
{
    SWSS_LOG_ENTER();

    sai_serialize_primitive(fdb->event_type, s);
    sai_serialize_primitive(fdb->fdb_entry, s);
    sai_serialize_primitive(fdb->attr_count, s);

    for (uint32_t i = 0; i < fdb->attr_count; i++)
    {
        const sai_attribute_t *attr = &fdb->attr[i];

        sai_serialize_primitive(attr->id, s);

        sai_attr_serialization_type_t serialization_type;
        sai_status_t status = sai_get_serialization_type(SAI_OBJECT_TYPE_FDB, attr->id, serialization_type);

        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("Unable to find serialization type for object type: %x and attribute id: %u, status: %u",
                    SAI_OBJECT_TYPE_FDB,
                    attr->id,
                    status);

            exit(EXIT_FAILURE);
        }

        status = sai_serialize_attr_value(serialization_type, *attr, s, false);

        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("Unable to serialize attribute for object type: %x and attribute id: %u, status: %u",
                SAI_OBJECT_TYPE_FDB,
                attr->id,
                status);

            exit(EXIT_FAILURE);
        }
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t sai_deserialize_fdb_event_notification_data(
        _In_ const std::string &s,
        _In_ int index,
        _Out_ sai_fdb_event_notification_data_t *fdb)
{
    SWSS_LOG_ENTER();

    sai_deserialize_primitive(s, index, fdb->event_type);
    sai_deserialize_primitive(s, index, fdb->fdb_entry);
    sai_deserialize_primitive(s, index, fdb->attr_count);

    fdb->attr = sai_alloc_n_of_ptr_type(fdb->attr_count, fdb->attr);

    for (uint32_t i = 0; i < fdb->attr_count; i++)
    {
        sai_attribute_t *attr = &fdb->attr[i];

        sai_deserialize_primitive(s, index, attr->id);

        sai_attr_serialization_type_t serialization_type;
        sai_status_t status = sai_get_serialization_type(SAI_OBJECT_TYPE_FDB, attr->id, serialization_type);

        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("Unable to find serialization type for object type: %x and attribute id: %u, status: %u",
                    SAI_OBJECT_TYPE_FDB,
                    attr->id,
                    status);

            return status;
        }

        status = sai_deserialize_attr_value(s, index, serialization_type, *attr, false);

        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("Unable to deserialize attribute for object type: %x and attribute id: %u, status: %u",
                SAI_OBJECT_TYPE_FDB,
                attr->id,
                status);

            return status;
        }
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t sai_deserialize_free_fdb_event_notification_data(
        _In_ sai_fdb_event_notification_data_t *fdb)
{
    SWSS_LOG_ENTER();

    // NOTE: on any failure we have memory leak since we don't
    // know which serialization type was used, we dont know what
    // should be deallocated

    for (uint32_t i = 0; i < fdb->attr_count; i++)
    {
        sai_attribute_t *attr = &fdb->attr[i];

        sai_attr_serialization_type_t serialization_type;
        sai_status_t status = sai_get_serialization_type(SAI_OBJECT_TYPE_FDB, attr->id, serialization_type);

        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("Unable to find serialization type for object type: %x and attribute id: %u, status: %u",
                    SAI_OBJECT_TYPE_FDB,
                    attr->id,
                    status);

            return status;
        }

        status = sai_deserialize_free_attribute_value(serialization_type, *attr);

        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("Unable to free attribute for object type: %x and attribute id: %u, status: %u",
                SAI_OBJECT_TYPE_FDB,
                attr->id,
                status);

            return status;
        }
    }

    sai_dealloc_array(fdb->attr);

    return SAI_STATUS_SUCCESS;
}

sai_status_t transfer_attribute(
        _In_ sai_attr_serialization_type_t serialization_type,
        _In_ sai_attribute_t &src_attr,
        _In_ sai_attribute_t &dst_attr,
        _In_ bool countOnly)
{
    switch (serialization_type)
    {
        case SAI_SERIALIZATION_TYPE_BOOL:
            transfer_primitive(src_attr.value.booldata, dst_attr.value.booldata);
            break;

        case SAI_SERIALIZATION_TYPE_CHARDATA:
            transfer_primitive(src_attr.value.chardata, dst_attr.value.chardata);
            break;

        case SAI_SERIALIZATION_TYPE_UINT8:
            transfer_primitive(src_attr.value.u8, dst_attr.value.u8);
            break;

        case SAI_SERIALIZATION_TYPE_INT8:
            transfer_primitive(src_attr.value.s8, dst_attr.value.s8);
            break;

        case SAI_SERIALIZATION_TYPE_UINT16:
            transfer_primitive(src_attr.value.u16, dst_attr.value.u16);
            break;

        case SAI_SERIALIZATION_TYPE_INT16:
            transfer_primitive(src_attr.value.s16, dst_attr.value.s16);
            break;

        case SAI_SERIALIZATION_TYPE_UINT32:
            transfer_primitive(src_attr.value.u32, dst_attr.value.u32);
            break;

        case SAI_SERIALIZATION_TYPE_INT32:
            transfer_primitive(src_attr.value.s32, dst_attr.value.s32);
            break;

        case SAI_SERIALIZATION_TYPE_UINT64:
            transfer_primitive(src_attr.value.u64, dst_attr.value.u64);
            break;

        case SAI_SERIALIZATION_TYPE_INT64:
            transfer_primitive(src_attr.value.s64, dst_attr.value.s64);
            break;

        case SAI_SERIALIZATION_TYPE_MAC:
            transfer_primitive(src_attr.value.mac, dst_attr.value.mac);
            break;

        case SAI_SERIALIZATION_TYPE_IP4:
            transfer_primitive(src_attr.value.ip4, dst_attr.value.ip4);
            break;

        case SAI_SERIALIZATION_TYPE_IP6:
            transfer_primitive(src_attr.value.ip6, dst_attr.value.ip6);
            break;

        case SAI_SERIALIZATION_TYPE_IP_ADDRESS:
            transfer_primitive(src_attr.value.ipaddr, dst_attr.value.ipaddr);
            break;

        case SAI_SERIALIZATION_TYPE_OBJECT_ID:
            transfer_primitive(src_attr.value.oid, dst_attr.value.oid);
            break;

        case SAI_SERIALIZATION_TYPE_OBJECT_LIST:
            transfer_list(src_attr.value.objlist, dst_attr.value.objlist, countOnly);
            break;

        case SAI_SERIALIZATION_TYPE_UINT8_LIST:
            transfer_list(src_attr.value.u8list, dst_attr.value.u8list, countOnly);
            break;

        case SAI_SERIALIZATION_TYPE_INT8_LIST:
            transfer_list(src_attr.value.s8list, dst_attr.value.s8list, countOnly);
            break;

        case SAI_SERIALIZATION_TYPE_UINT16_LIST:
            transfer_list(src_attr.value.u16list, dst_attr.value.u16list, countOnly);
            break;

        case SAI_SERIALIZATION_TYPE_INT16_LIST:
            transfer_list(src_attr.value.s16list, dst_attr.value.s16list, countOnly);
            break;

        case SAI_SERIALIZATION_TYPE_UINT32_LIST:
            transfer_list(src_attr.value.u32list, dst_attr.value.u32list, countOnly);
            break;

        case SAI_SERIALIZATION_TYPE_INT32_LIST:
            transfer_list(src_attr.value.s32list, dst_attr.value.s32list, countOnly);
            break;

        case SAI_SERIALIZATION_TYPE_UINT32_RANGE:
            transfer_primitive(src_attr.value.u32range, dst_attr.value.u32range);
            break;

        case SAI_SERIALIZATION_TYPE_INT32_RANGE:
            transfer_primitive(src_attr.value.s32range, dst_attr.value.s32range);
            break;

        case SAI_SERIALIZATION_TYPE_VLAN_LIST:
            transfer_list(src_attr.value.vlanlist, dst_attr.value.vlanlist, countOnly);
            break;

        case SAI_SERIALIZATION_TYPE_PORT_BREAKOUT:
            transfer_primitive(src_attr.value.portbreakout.breakout_mode, dst_attr.value.portbreakout.breakout_mode);
            transfer_list(src_attr.value.portbreakout.port_list, dst_attr.value.portbreakout.port_list, countOnly);
            break;

        case SAI_SERIALIZATION_TYPE_QOS_MAP_LIST:
            transfer_list(src_attr.value.qosmap, dst_attr.value.qosmap, countOnly);
            break;

        case SAI_SERIALIZATION_TYPE_TUNNEL_MAP_LIST:
            transfer_list(src_attr.value.tunnelmap, dst_attr.value.tunnelmap, countOnly);
            break;

            /* ACL FIELD DATA */

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_BOOL:
            transfer_primitive(src_attr.value.aclfield.enable, dst_attr.value.aclfield.enable);
            transfer_primitive(src_attr.value.aclfield.data.booldata, dst_attr.value.aclfield.data.booldata);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT8:
            transfer_primitive(src_attr.value.aclfield.enable, dst_attr.value.aclfield.enable);
            transfer_primitive(src_attr.value.aclfield.mask.u8, dst_attr.value.aclfield.mask.u8);
            transfer_primitive(src_attr.value.aclfield.data.u8, dst_attr.value.aclfield.data.u8);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_INT8:
            transfer_primitive(src_attr.value.aclfield.enable, dst_attr.value.aclfield.enable);
            transfer_primitive(src_attr.value.aclfield.mask.s8, dst_attr.value.aclfield.mask.s8);
            transfer_primitive(src_attr.value.aclfield.data.s8, dst_attr.value.aclfield.data.s8);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT16:
            transfer_primitive(src_attr.value.aclfield.enable, dst_attr.value.aclfield.enable);
            transfer_primitive(src_attr.value.aclfield.mask.u16, dst_attr.value.aclfield.mask.u16);
            transfer_primitive(src_attr.value.aclfield.data.u16, dst_attr.value.aclfield.data.u16);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_INT16:
            transfer_primitive(src_attr.value.aclfield.enable, dst_attr.value.aclfield.enable);
            transfer_primitive(src_attr.value.aclfield.mask.s16, dst_attr.value.aclfield.mask.s16);
            transfer_primitive(src_attr.value.aclfield.data.s16, dst_attr.value.aclfield.data.s16);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT32:
            transfer_primitive(src_attr.value.aclfield.enable, dst_attr.value.aclfield.enable);
            transfer_primitive(src_attr.value.aclfield.mask.u16, dst_attr.value.aclfield.mask.u16);
            transfer_primitive(src_attr.value.aclfield.data.u16, dst_attr.value.aclfield.data.u16);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_INT32:
            transfer_primitive(src_attr.value.aclfield.enable, dst_attr.value.aclfield.enable);
            transfer_primitive(src_attr.value.aclfield.mask.s32, dst_attr.value.aclfield.mask.s32);
            transfer_primitive(src_attr.value.aclfield.data.s32, dst_attr.value.aclfield.data.s32);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_MAC:
            transfer_primitive(src_attr.value.aclfield.enable, dst_attr.value.aclfield.enable);
            transfer_primitive(src_attr.value.aclfield.mask.mac, dst_attr.value.aclfield.mask.mac);
            transfer_primitive(src_attr.value.aclfield.data.mac, dst_attr.value.aclfield.data.mac);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_IP4:
            transfer_primitive(src_attr.value.aclfield.enable, dst_attr.value.aclfield.enable);
            transfer_primitive(src_attr.value.aclfield.mask.ip4, dst_attr.value.aclfield.mask.ip4);
            transfer_primitive(src_attr.value.aclfield.data.ip4, dst_attr.value.aclfield.data.ip4);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_IP6:
            transfer_primitive(src_attr.value.aclfield.enable, dst_attr.value.aclfield.enable);
            transfer_primitive(src_attr.value.aclfield.mask.ip6, dst_attr.value.aclfield.mask.ip6);
            transfer_primitive(src_attr.value.aclfield.data.ip6, dst_attr.value.aclfield.data.ip6);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_OBJECT_ID:
            transfer_primitive(src_attr.value.aclfield.enable, dst_attr.value.aclfield.enable);
            transfer_primitive(src_attr.value.aclfield.data.oid, dst_attr.value.aclfield.data.oid);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_OBJECT_LIST:
            transfer_primitive(src_attr.value.aclfield.enable, dst_attr.value.aclfield.enable);
            transfer_list(src_attr.value.aclfield.data.objlist, dst_attr.value.aclfield.data.objlist, countOnly);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT8_LIST:
            transfer_primitive(src_attr.value.aclfield.enable, dst_attr.value.aclfield.enable);
            transfer_list(src_attr.value.aclfield.mask.u8list, dst_attr.value.aclfield.mask.u8list, countOnly);
            transfer_list(src_attr.value.aclfield.data.u8list, dst_attr.value.aclfield.data.u8list, countOnly);
            break;

            /* ACL ACTION DATA */

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_UINT8:
            transfer_primitive(src_attr.value.aclaction.enable, dst_attr.value.aclaction.enable);
            transfer_primitive(src_attr.value.aclaction.parameter.u8, dst_attr.value.aclaction.parameter.u8);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_INT8:
            transfer_primitive(src_attr.value.aclaction.enable, dst_attr.value.aclaction.enable);
            transfer_primitive(src_attr.value.aclaction.parameter.s8, dst_attr.value.aclaction.parameter.s8);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_UINT16:
            transfer_primitive(src_attr.value.aclaction.enable, dst_attr.value.aclaction.enable);
            transfer_primitive(src_attr.value.aclaction.parameter.u16, dst_attr.value.aclaction.parameter.u16);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_INT16:
            transfer_primitive(src_attr.value.aclaction.enable, dst_attr.value.aclaction.enable);
            transfer_primitive(src_attr.value.aclaction.parameter.s16, dst_attr.value.aclaction.parameter.s16);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_UINT32:
            transfer_primitive(src_attr.value.aclaction.enable, dst_attr.value.aclaction.enable);
            transfer_primitive(src_attr.value.aclaction.parameter.u32, dst_attr.value.aclaction.parameter.u32);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_INT32:
            transfer_primitive(src_attr.value.aclaction.enable, dst_attr.value.aclaction.enable);
            transfer_primitive(src_attr.value.aclaction.parameter.s32, dst_attr.value.aclaction.parameter.s32);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_MAC:
            transfer_primitive(src_attr.value.aclaction.enable, dst_attr.value.aclaction.enable);
            transfer_primitive(src_attr.value.aclaction.parameter.mac, dst_attr.value.aclaction.parameter.mac);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_IPV4:
            transfer_primitive(src_attr.value.aclaction.enable, dst_attr.value.aclaction.enable);
            transfer_primitive(src_attr.value.aclaction.parameter.ip4, dst_attr.value.aclaction.parameter.ip4);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_IPV6:
            transfer_primitive(src_attr.value.aclaction.enable, dst_attr.value.aclaction.enable);
            transfer_primitive(src_attr.value.aclaction.parameter.ip6, dst_attr.value.aclaction.parameter.ip6);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_ID:
            transfer_primitive(src_attr.value.aclaction.enable, dst_attr.value.aclaction.enable);
            transfer_primitive(src_attr.value.aclaction.parameter.oid, dst_attr.value.aclaction.parameter.oid);
            break;

        case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_LIST:
            transfer_primitive(src_attr.value.aclaction.enable, dst_attr.value.aclaction.enable);
            transfer_list(src_attr.value.aclaction.parameter.objlist, dst_attr.value.aclaction.parameter.objlist, countOnly);
            break;

        default:
            return SAI_STATUS_NOT_IMPLEMENTED;
    }

    return SAI_STATUS_SUCCESS;
}

void transfer_attributes(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *src_attr_list,
        _In_ sai_attribute_t *dst_attr_list,
        _In_ bool countOnly)
{
    for (uint32_t i = 0; i < attr_count; i++)
    {
        sai_attribute_t &src_attr = src_attr_list[i];
        sai_attribute_t &dst_attr = dst_attr_list[i];

        sai_attr_serialization_type_t serialization_type;
        sai_status_t status = sai_get_serialization_type(object_type, src_attr.id, serialization_type);

        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("Unable to find serialization type object type: %x and attribute id: %u, status: %u",
                SAI_OBJECT_TYPE_FDB,
                src_attr.id,
                status);

            exit(EXIT_FAILURE);
        }

        transfer_attribute(serialization_type, src_attr, dst_attr, countOnly);
    }
}

void sai_serialize_ip_address(
        _In_ const sai_ip_address_t &ip_address,
        _Out_ std::string &s)
{
    sai_serialize_primitive(ip_address.addr_family, s);

    if (ip_address.addr_family == SAI_IP_ADDR_FAMILY_IPV4)
        sai_serialize_primitive(ip_address.addr.ip4, s);

    if (ip_address.addr_family == SAI_IP_ADDR_FAMILY_IPV6)
        sai_serialize_primitive(ip_address.addr.ip6, s);
}

void sai_serialize_neighbor_entry(
        _In_ const sai_neighbor_entry_t &ne,
        _Out_ std::string &s)
{
    sai_serialize_primitive(ne.rif_id, s);

    sai_serialize_ip_address(ne.ip_address, s);
}

void sai_deserialize_ip_address(
        _In_ const std::string & s,
        _In_ int &index,
        _Out_ sai_ip_address_t &ip_address)
{
    sai_deserialize_primitive(s, index, ip_address.addr_family);

    if (ip_address.addr_family == SAI_IP_ADDR_FAMILY_IPV4)
        sai_deserialize_primitive(s, index, ip_address.addr.ip4);

    if (ip_address.addr_family == SAI_IP_ADDR_FAMILY_IPV6)
        sai_deserialize_primitive(s, index, ip_address.addr.ip6);
}

void sai_deserialize_neighbor_entry(
        _In_ const std::string & s,
        _In_ int &index,
        _Out_ sai_neighbor_entry_t &ne)
{
    sai_deserialize_primitive(s, index, ne.rif_id);

    sai_deserialize_ip_address(s, index, ne.ip_address);
}

void sai_serialize_ip_prefix(
        _In_ const sai_ip_prefix_t &ip_prefix,
        _Out_ std::string &s)
{
    sai_serialize_primitive(ip_prefix.addr_family, s);

    if (ip_prefix.addr_family == SAI_IP_ADDR_FAMILY_IPV4)
    {
        sai_serialize_primitive(ip_prefix.addr.ip4, s);
        sai_serialize_primitive(ip_prefix.mask.ip4, s);
    }

    if (ip_prefix.addr_family == SAI_IP_ADDR_FAMILY_IPV6)
    {
        sai_serialize_primitive(ip_prefix.addr.ip6, s);
        sai_serialize_primitive(ip_prefix.mask.ip6, s);
    }
}

void sai_serialize_route_entry(
        _In_ const sai_unicast_route_entry_t &re,
        _Out_ std::string &s)
{
    sai_serialize_primitive(re.vr_id, s);

    sai_serialize_ip_prefix(re.destination, s);
}

void sai_deserialize_ip_prefix(
        _In_ const std::string & s,
        _In_ int &index,
        _Out_ sai_ip_prefix_t &ip_prefix)
{
    sai_deserialize_primitive(s, index, ip_prefix.addr_family);

    if (ip_prefix.addr_family == SAI_IP_ADDR_FAMILY_IPV4)
    {
        sai_deserialize_primitive(s, index, ip_prefix.addr.ip4);
        sai_deserialize_primitive(s, index, ip_prefix.mask.ip4);
    }

    if (ip_prefix.addr_family == SAI_IP_ADDR_FAMILY_IPV6)
    {
        sai_deserialize_primitive(s, index, ip_prefix.addr.ip6);
        sai_deserialize_primitive(s, index, ip_prefix.mask.ip6);
    }
}

void sai_deserialize_route_entry(
        _In_ const std::string & s,
        _In_ int &index,
        _Out_ sai_unicast_route_entry_t &re)
{
    sai_deserialize_primitive(s, index, re.vr_id);

    sai_deserialize_ip_prefix(s, index, re.destination);
}
