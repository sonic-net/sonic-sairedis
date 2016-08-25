#include "sai_redis.h"
#include <thread>

#include "swss/selectableevent.h"

std::set<sai_object_id_t> local_switches_set;

// TODO only until switch will be actual object
#define DEFAULT_SWITCH_ID 0

sai_object_id_t local_cpu_port_id = SAI_NULL_OBJECT_ID;
sai_object_id_t local_default_trap_group_id = SAI_NULL_OBJECT_ID;

// TODO it may be needed to obtain SAI_SWITCH_ATTR_DEFAULT_TRAP_GROUP object id

// if we will not get response in 60 seconds when
// notify syncd to compile new state or to switch
// to compiled state, then there is something wrong
#define NOTIFY_SYNCD_TIMEOUT (60*1000)

#define NOTIFY_SAI_INIT_VIEW    "SAI_INIT_VIEW"
#define NOTIFY_SAI_APPLY_VIEW   "SAI_APPLY_VIEW"

sai_switch_notification_t redis_switch_notifications;

bool g_switchInitialized = false;
volatile bool g_run = false;

std::shared_ptr<std::thread> notification_thread;

// this event is used to nice end notifications thread
swss::SelectableEvent g_redisNotificationTrheadEvent;

void ntf_thread()
{
    SWSS_LOG_ENTER();

    swss::Select s;

    s.addSelectable(g_redisNotifications);
    s.addSelectable(&g_redisNotificationTrheadEvent);

    while (g_run)
    {
        swss::Selectable *sel;

        int fd;

        int result = s.select(&sel, &fd);

        if (sel == &g_redisNotificationTrheadEvent)
        {
            // user requested shutdown_switch
            break;
        }

        if (result == swss::Select::OBJECT)
        {
            swss::KeyOpFieldsValuesTuple kco;

            std::string op;
            std::string data;
            std::vector<swss::FieldValueTuple> values;

            g_redisNotifications->pop(op, data, values);

            SWSS_LOG_DEBUG("notification: op = %s, data = %s", op.c_str(), data.c_str());

            handle_notification(op, data, values);
        }
    }
}

sai_status_t notify_syncd(const std::string &operation)
{
    SWSS_LOG_ENTER();

    std::vector<swss::FieldValueTuple> entry;

    g_notifySyncdProducer->send(operation, "", entry);

    swss::Select s;

    s.addSelectable(g_notifySyncdConsumer);

    SWSS_LOG_DEBUG("wait for response after: %s", operation.c_str());

    swss::Selectable *sel;

    int fd;

    int result = s.select(&sel, &fd, NOTIFY_SYNCD_TIMEOUT);

    if (result == swss::Select::OBJECT)
    {
        swss::KeyOpFieldsValuesTuple kco;

        std::string op;
        std::string data;
        std::vector<swss::FieldValueTuple> values;

        g_notifySyncdConsumer->pop(op, data, values);

        const std::string &strStatus = op;

        sai_status_t status;

        int index = 0;
        sai_deserialize_primitive(strStatus, index, status);

        SWSS_LOG_NOTICE("%s status: %d", op.c_str(), status);

        return status;
    }

    SWSS_LOG_ERROR("%s get response failed, result: %d", operation.c_str(), result);

    return SAI_STATUS_FAILURE;
}

void populate_hostif_traps()
{
    local_hostif_traps_set.insert(SAI_HOSTIF_TRAP_ID_STP);
    local_hostif_traps_set.insert(SAI_HOSTIF_TRAP_ID_LACP);
    local_hostif_traps_set.insert(SAI_HOSTIF_TRAP_ID_EAPOL);
    local_hostif_traps_set.insert(SAI_HOSTIF_TRAP_ID_LLDP);
    local_hostif_traps_set.insert(SAI_HOSTIF_TRAP_ID_PVRST);
    local_hostif_traps_set.insert(SAI_HOSTIF_TRAP_ID_IGMP_TYPE_QUERY);
    local_hostif_traps_set.insert(SAI_HOSTIF_TRAP_ID_IGMP_TYPE_LEAVE);
    local_hostif_traps_set.insert(SAI_HOSTIF_TRAP_ID_IGMP_TYPE_V1_REPORT);
    local_hostif_traps_set.insert(SAI_HOSTIF_TRAP_ID_IGMP_TYPE_V2_REPORT);
    local_hostif_traps_set.insert(SAI_HOSTIF_TRAP_ID_IGMP_TYPE_V3_REPORT);
    local_hostif_traps_set.insert(SAI_HOSTIF_TRAP_ID_SAMPLEPACKET);
    local_hostif_traps_set.insert(SAI_HOSTIF_TRAP_ID_SWITCH_CUSTOM_RANGE_BASE);
    local_hostif_traps_set.insert(SAI_HOSTIF_TRAP_ID_ARP_REQUEST);
    local_hostif_traps_set.insert(SAI_HOSTIF_TRAP_ID_ARP_RESPONSE);
    local_hostif_traps_set.insert(SAI_HOSTIF_TRAP_ID_DHCP);
    local_hostif_traps_set.insert(SAI_HOSTIF_TRAP_ID_OSPF);
    local_hostif_traps_set.insert(SAI_HOSTIF_TRAP_ID_PIM);
    local_hostif_traps_set.insert(SAI_HOSTIF_TRAP_ID_VRRP);
    local_hostif_traps_set.insert(SAI_HOSTIF_TRAP_ID_BGP);
    local_hostif_traps_set.insert(SAI_HOSTIF_TRAP_ID_DHCPV6);
    local_hostif_traps_set.insert(SAI_HOSTIF_TRAP_ID_OSPFV6);
    local_hostif_traps_set.insert(SAI_HOSTIF_TRAP_ID_VRRPV6);
    local_hostif_traps_set.insert(SAI_HOSTIF_TRAP_ID_BGPV6);
    local_hostif_traps_set.insert(SAI_HOSTIF_TRAP_ID_IPV6_NEIGHBOR_DISCOVERY);
    local_hostif_traps_set.insert(SAI_HOSTIF_TRAP_ID_IPV6_MLD_V1_V2);
    local_hostif_traps_set.insert(SAI_HOSTIF_TRAP_ID_IPV6_MLD_V1_REPORT);
    local_hostif_traps_set.insert(SAI_HOSTIF_TRAP_ID_IPV6_MLD_V1_DONE);
    local_hostif_traps_set.insert(SAI_HOSTIF_TRAP_ID_MLD_V2_REPORT);
    local_hostif_traps_set.insert(SAI_HOSTIF_TRAP_ID_IP2ME);
    local_hostif_traps_set.insert(SAI_HOSTIF_TRAP_ID_SSH);
    local_hostif_traps_set.insert(SAI_HOSTIF_TRAP_ID_SNMP);
    local_hostif_traps_set.insert(SAI_HOSTIF_TRAP_ID_ROUTER_CUSTOM_RANGE_BASE);
    local_hostif_traps_set.insert(SAI_HOSTIF_TRAP_ID_L3_MTU_ERROR);
    local_hostif_traps_set.insert(SAI_HOSTIF_TRAP_ID_TTL_ERROR);
}

void clear_local_state()
{
    local_lag_members_set.clear();
    local_lags_set.clear();
    local_neighbor_entries_set.clear();
    local_next_hop_groups_set.clear();
    local_next_hops_set.clear();
    local_policers_set.clear();
    local_ports_set.clear();
    local_route_entries_set.clear();
    local_router_interfaces_set.clear();
    local_switches_set.clear();
    local_tunnel_maps_set.clear();
    local_tunnels_set.clear();
    local_tunnel_term_table_entries_set.clear();
    local_virtual_routers_set.clear();
    local_vlan_members_set.clear();
    local_hostifs_set.clear();
    local_hostif_trap_groups_set.clear();
    local_hostif_traps_set.clear();
    local_vlans_set.clear();
    local_user_defined_hostif_traps_set.clear();
    local_wreds_set.clear();

    // populate default objects

    local_vlans_set.insert(DEFAULT_VLAN_NUMBER);

    populate_hostif_traps();

    local_switches_set.insert(DEFAULT_SWITCH_ID);

    // TODO populate vlan 1 members via ports ? get from switch?
    // same from default router and cou port id should be obtained from switch

    // TODO populate ports list

    local_default_virtual_router_id = SAI_NULL_OBJECT_ID;
    local_default_trap_group_id = SAI_NULL_OBJECT_ID; // TODO needs to be snooped
    local_cpu_port_id = SAI_NULL_OBJECT_ID;

    // TODO getting default object id's like TRAP which can contain other object
    // id as dependency (in this case queue/policer)
    // we need to also GET those attributes at first switch init to have full
    // asic view of what's goind on and also we need extra logic on remove
    // function (we can add 4 pointers in metadata c/r/s/g) because some default
    // object's can't be removed like cpu port or default virtual router
    // and probably default trap group
}

/**
* Routine Description:
*   SDK initialization. After the call the capability attributes should be
*   ready for retrieval via sai_get_switch_attribute().
*
* Arguments:
*   @param[in] profile_id - Handle for the switch profile.
*   @param[in] switch_hardware_id - Switch hardware ID to open
*   @param[in] firmware_path_name - Vendor specific path name of the firmware
*                                   to load
*   @param[in] switch_notifications - switch notification table
* Return Values:
*   @return SAI_STATUS_SUCCESS on success
*           Failure status code on error
*/
sai_status_t redis_initialize_switch(
    _In_ sai_switch_profile_id_t profile_id,
    _In_reads_z_(SAI_MAX_HARDWARE_ID_LEN) char* switch_hardware_id,
    _In_reads_opt_z_(SAI_MAX_FIRMWARE_PATH_NAME_LEN) char* firmware_path_name,
    _In_ sai_switch_notification_t* switch_notifications)
{
    std::lock_guard<std::mutex> apilock(g_apimutex);

    std::lock_guard<std::mutex> lock(g_mutex);

    SWSS_LOG_ENTER();

    if (firmware_path_name == NULL)
    {
        SWSS_LOG_ERROR("firmware path name is NULL");

        return SAI_STATUS_FAILURE;
    }

    std::string op = std::string(firmware_path_name);

    SWSS_LOG_NOTICE("operation: '%s'", op.c_str());

    if (op == NOTIFY_SAI_INIT_VIEW || op == NOTIFY_SAI_APPLY_VIEW)
    {
        sai_status_t status = notify_syncd(op);

        if (status == SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_NOTICE("sending %s to syncd succeeded", op.c_str());

            if (g_switchInitialized)
            {
                if (op == NOTIFY_SAI_INIT_VIEW)
                {
                    SWSS_LOG_NOTICE("clearing current local state sinice init view is called on initialised switch");

                    // TODO since we clear all defaults here and we are compiling new view
                    // there may be some problems with GET
                    clear_local_state();
                }

                return status;
            }

            // proceed with proper initialization
        }
        else
        {
            SWSS_LOG_ERROR("sending %s to syncd failed: %d", op.c_str(), status);

            return status;
        }
    }
    else
    {
        SWSS_LOG_WARN("unknown operation: '%s'", op.c_str());
    }

    if (g_switchInitialized)
    {
        SWSS_LOG_ERROR("switch is already initialized");

        return SAI_STATUS_FAILURE;
    }

    g_switchInitialized = true;

    if (switch_notifications != NULL)
    {
        memcpy(&redis_switch_notifications,
               switch_notifications,
               sizeof(sai_switch_notification_t));
    }
    else
    {
        memset(&redis_switch_notifications, 0, sizeof(sai_switch_notification_t));
    }

    clear_local_state();

    g_run = true;

    SWSS_LOG_DEBUG("creating notification thread");

    notification_thread = std::make_shared<std::thread>(std::thread(ntf_thread));

    return SAI_STATUS_SUCCESS;
}

/**
 * Routine Description:
 *   @brief Release all resources associated with currently opened switch
 *
 * Arguments:
 *   @param[in] warm_restart_hint - hint that indicates controlled warm restart.
 *                            Since warm restart can be caused by crash
 *                            (therefore there are no guarantees for this call),
 *                            this hint is really a performance optimization.
 *
 * Return Values:
 *   None
 */
void redis_shutdown_switch(
    _In_ bool warm_restart_hint)
{
    std::lock_guard<std::mutex> apilock(g_apimutex);

    std::lock_guard<std::mutex> lock(g_mutex);

    SWSS_LOG_ENTER();

    if (!g_switchInitialized)
    {
        SWSS_LOG_ERROR("not initialized");

        return;
    }

    g_run = false;

    // notify thread that it should end
    g_redisNotificationTrheadEvent.notify();

    notification_thread->join();

    g_switchInitialized = false;

    memset(&redis_switch_notifications, 0, sizeof(sai_switch_notification_t));
}

/**
 * Routine Description:
 *   SDK connect. This API connects library to the initialized SDK.
 *   After the call the capability attributes should be ready for retrieval
 *   via sai_get_switch_attribute().
 *
 * Arguments:
 *   @param[in] profile_id - Handle for the switch profile.
 *   @param[in] switch_hardware_id - Switch hardware ID to open
 *   @param[in] switch_notifications - switch notification table
 * Return Values:
 *   @return SAI_STATUS_SUCCESS on success
 *           Failure status code on error
 */
sai_status_t redis_connect_switch(
    _In_ sai_switch_profile_id_t profile_id,
    _In_reads_z_(SAI_MAX_HARDWARE_ID_LEN) char* switch_hardware_id,
    _In_ sai_switch_notification_t* switch_notifications)
{
    std::lock_guard<std::mutex> apilock(g_apimutex);

    std::lock_guard<std::mutex> lock(g_mutex);

    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");

    return SAI_STATUS_NOT_IMPLEMENTED;
}

/**
 * Routine Description:
 *   @brief Disconnect this SAI library from the SDK.
 *
 * Arguments:
 *   None
 * Return Values:
 *   None
 */
void redis_disconnect_switch(void)
{
    std::lock_guard<std::mutex> apilock(g_apimutex);

    std::lock_guard<std::mutex> lock(g_mutex);

    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("not implemented");
}

/**
 * Routine Description:
 *    @brief Set switch attribute value
 *
 * Arguments:
 *    @param[in] attr - switch attribute
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_set_switch_attribute(
    _In_ const sai_attribute_t *attr)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr == NULL)
    {
        SWSS_LOG_ERROR("attribute parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // TODO use proper switch ID when switch will be an object
    sai_object_id_t switch_id = DEFAULT_SWITCH_ID;

    if (local_switches_set.find(switch_id) == local_switches_set.end())
    {
        SWSS_LOG_ERROR("switch %llx is missing", switch_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    switch (attr->id)
    {
        // TODO commented stuff needs to check for right object existence

        case SAI_SWITCH_ATTR_SWITCHING_MODE:

            {
                sai_switch_switching_mode_t mode = (sai_switch_switching_mode_t)attr->value.s32;

                switch (mode)
                {
                    case SAI_SWITCHING_MODE_CUT_THROUGH:
                    case SAI_SWITCHING_MODE_STORE_AND_FORWARD:
                        // ok
                        break;

                    default:

                        SWSS_LOG_ERROR("invalid switching mode value: %d", mode);

                        return SAI_STATUS_INVALID_PARAMETER;
                }
            }

            break;

        case SAI_SWITCH_ATTR_BCAST_CPU_FLOOD_ENABLE:
        case SAI_SWITCH_ATTR_MCAST_CPU_FLOOD_ENABLE:
        case SAI_SWITCH_ATTR_SRC_MAC_ADDRESS:
        case SAI_SWITCH_ATTR_MAX_LEARNED_ADDRESSES:
        case SAI_SWITCH_ATTR_FDB_AGING_TIME:
            break;

        case SAI_SWITCH_ATTR_FDB_UNICAST_MISS_ACTION:
        case SAI_SWITCH_ATTR_FDB_BROADCAST_MISS_ACTION:
        case SAI_SWITCH_ATTR_FDB_MULTICAST_MISS_ACTION:

            {
                sai_packet_action_t action = (sai_packet_action_t)attr->value.s32;

                switch (action)
                {
                    case SAI_PACKET_ACTION_DROP:
                    case SAI_PACKET_ACTION_FORWARD:
                    case SAI_PACKET_ACTION_COPY:
                    case SAI_PACKET_ACTION_COPY_CANCEL:
                    case SAI_PACKET_ACTION_TRAP:
                    case SAI_PACKET_ACTION_LOG:
                    case SAI_PACKET_ACTION_DENY:
                    case SAI_PACKET_ACTION_TRANSIT:
                        // ok
                        break;

                    default:

                        SWSS_LOG_ERROR("invalid packet action value: %d", action);

                        return SAI_STATUS_INVALID_PARAMETER;
                }
            }

            break;

        case SAI_SWITCH_ATTR_ECMP_DEFAULT_HASH_ALGORITHM:

            {
                sai_hash_algorithm_t hash_algorithm = (sai_hash_algorithm_t)attr->value.s32;

                switch (hash_algorithm)
                {
                    case SAI_HASH_ALGORITHM_CRC:
                    case SAI_HASH_ALGORITHM_XOR:
                    case SAI_HASH_ALGORITHM_RANDOM:
                        // ok
                        break;

                    default:

                        SWSS_LOG_ERROR("invalid ecmp default hash algorithm value: %d", hash_algorithm);

                        return SAI_STATUS_INVALID_PARAMETER;
                }
            }

            break;

        case SAI_SWITCH_ATTR_ECMP_DEFAULT_HASH_SEED:
        case SAI_SWITCH_ATTR_ECMP_DEFAULT_SYMMETRIC_HASH:
        // case SAI_SWITCH_ATTR_ECMP_HASH_IPV4:
        // case SAI_SWITCH_ATTR_ECMP_HASH_IPV4_IN_IPV4:
        // case SAI_SWITCH_ATTR_ECMP_HASH_IPV6:
        case SAI_SWITCH_ATTR_LAG_DEFAULT_HASH_ALGORITHM:
        case SAI_SWITCH_ATTR_LAG_DEFAULT_HASH_SEED:
        case SAI_SWITCH_ATTR_LAG_DEFAULT_SYMMETRIC_HASH:
        // case SAI_SWITCH_ATTR_LAG_HASH_IPV4:
        // case SAI_SWITCH_ATTR_LAG_HASH_IPV4_IN_IPV4:
        // case SAI_SWITCH_ATTR_LAG_HASH_IPV6:
        case SAI_SWITCH_ATTR_COUNTER_REFRESH_INTERVAL:
        //case SAI_SWITCH_ATTR_QOS_DEFAULT_TC:
        //case SAI_SWITCH_ATTR_QOS_DOT1P_TO_TC_MAP:
        //case SAI_SWITCH_ATTR_QOS_DOT1P_TO_COLOR_MAP:
        //case SAI_SWITCH_ATTR_QOS_DSCP_TO_TC_MAP:
        //case SAI_SWITCH_ATTR_QOS_DSCP_TO_COLOR_MAP:
        //case SAI_SWITCH_ATTR_QOS_TC_TO_QUEUE_MAP:
        //case SAI_SWITCH_ATTR_QOS_TC_AND_COLOR_TO_DOT1P_MAP:
        //case SAI_SWITCH_ATTR_QOS_TC_AND_COLOR_TO_DSCP_MAP:
            // ok
            break;

        default:

            SWSS_LOG_ERROR("setting attribute id %d is not supported", attr->id);

            return SAI_STATUS_INVALID_PARAMETER;
    }


    sai_status_t status = redis_generic_set(
            SAI_OBJECT_TYPE_SWITCH,
            (sai_object_id_t)0, // dummy sai_object_id_t for switch
            attr);

    return status;
}

/**
 * Routine Description:
 *    @brief Get switch attribute value
 *
 * Arguments:
 *    @param[in] attr_count - number of switch attributes
 *    @param[inout] attr_list - array of switch attributes
 *
 * Return Values:
 *    @return SAI_STATUS_SUCCESS on success
 *            Failure status code on error
 */
sai_status_t redis_get_switch_attribute(
    _In_ sai_uint32_t attr_count,
    _Inout_ sai_attribute_t *attr_list)
{
    std::lock_guard<std::mutex> lock(g_apimutex);

    SWSS_LOG_ENTER();

    if (attr_list == NULL)
    {
        SWSS_LOG_ERROR("attribute list parameter is NULL");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr_count < 1)
    {
        SWSS_LOG_ERROR("attribute count must be at least 1");

        return SAI_STATUS_INVALID_PARAMETER;
    }

    // TODO use proper switch ID when switch will be an object
    sai_object_id_t switch_id = DEFAULT_SWITCH_ID;

    if (local_switches_set.find(switch_id) == local_switches_set.end())
    {
        SWSS_LOG_ERROR("switch %llx is missing", switch_id);

        return SAI_STATUS_INVALID_PARAMETER;
    }

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        sai_attribute_t* attr = &attr_list[i];

        switch (attr->id)
        {
            // RO
            case SAI_SWITCH_ATTR_PORT_NUMBER:
                break;

            case SAI_SWITCH_ATTR_PORT_LIST:

                {
                    sai_object_list_t port_list = attr->value.objlist;

                    if (port_list.list == NULL)
                    {
                        SWSS_LOG_ERROR("port list is NULL");

                        return SAI_STATUS_INVALID_PARAMETER;
                    }
                }

                break;

            case SAI_SWITCH_ATTR_PORT_MAX_MTU:
            case SAI_SWITCH_ATTR_CPU_PORT:
            case SAI_SWITCH_ATTR_MAX_VIRTUAL_ROUTERS:
            case SAI_SWITCH_ATTR_FDB_TABLE_SIZE:
            case SAI_SWITCH_ATTR_L3_NEIGHBOR_TABLE_SIZE:
            case SAI_SWITCH_ATTR_L3_ROUTE_TABLE_SIZE:
            case SAI_SWITCH_ATTR_LAG_MEMBERS:
            case SAI_SWITCH_ATTR_NUMBER_OF_LAGS:
            case SAI_SWITCH_ATTR_ECMP_MEMBERS:
            case SAI_SWITCH_ATTR_NUMBER_OF_ECMP_GROUPS:
            case SAI_SWITCH_ATTR_NUMBER_OF_UNICAST_QUEUES:
            case SAI_SWITCH_ATTR_NUMBER_OF_MULTICAST_QUEUES:
            case SAI_SWITCH_ATTR_NUMBER_OF_QUEUES:
            case SAI_SWITCH_ATTR_NUMBER_OF_CPU_QUEUES:
            case SAI_SWITCH_ATTR_ON_LINK_ROUTE_SUPPORTED:
            case SAI_SWITCH_ATTR_OPER_STATUS:
            case SAI_SWITCH_ATTR_MAX_TEMP:
            case SAI_SWITCH_ATTR_ACL_TABLE_MINIMUM_PRIORITY:
            case SAI_SWITCH_ATTR_ACL_TABLE_MAXIMUM_PRIORITY:
            case SAI_SWITCH_ATTR_ACL_ENTRY_MINIMUM_PRIORITY:
            case SAI_SWITCH_ATTR_ACL_ENTRY_MAXIMUM_PRIORITY:
            case SAI_SWITCH_ATTR_FDB_DST_USER_META_DATA_RANGE:
            case SAI_SWITCH_ATTR_ROUTE_DST_USER_META_DATA_RANGE:
            case SAI_SWITCH_ATTR_NEIGHBOR_DST_USER_META_DATA_RANGE:
            case SAI_SWITCH_ATTR_PORT_USER_META_DATA_RANGE:
            case SAI_SWITCH_ATTR_VLAN_USER_META_DATA_RANGE:
            case SAI_SWITCH_ATTR_ACL_USER_META_DATA_RANGE:
            case SAI_SWITCH_ATTR_ACL_USER_TRAP_ID_RANGE:
            case SAI_SWITCH_ATTR_DEFAULT_STP_INST_ID:
            case SAI_SWITCH_ATTR_DEFAULT_VIRTUAL_ROUTER_ID:
            case SAI_SWITCH_ATTR_QOS_MAX_NUMBER_OF_TRAFFIC_CLASSES:
            case SAI_SWITCH_ATTR_QOS_MAX_NUMBER_OF_SCHEDULER_GROUP_HIERARCHY_LEVELS:
                break;

            case SAI_SWITCH_ATTR_QOS_MAX_NUMBER_OF_SCHEDULER_GROUPS_PER_HIERARCHY_LEVEL: // sai_u32_li

                {
                    sai_u32_list_t groups_per_hierarhy = attr->value.u32list;

                    if (groups_per_hierarhy.list == NULL)
                    {
                        SWSS_LOG_ERROR("sheduler groups per hierarhy level list is NULL");

                        return SAI_STATUS_INVALID_PARAMETER;
                    }
                }

                break;

            case SAI_SWITCH_ATTR_QOS_MAX_NUMBER_OF_CHILDS_PER_SCHEDULER_GROUP:
            case SAI_SWITCH_ATTR_TOTAL_BUFFER_SIZE:
            case SAI_SWITCH_ATTR_INGRESS_BUFFER_POOL_NUM:
            case SAI_SWITCH_ATTR_EGRESS_BUFFER_POOL_NUM:
            case SAI_SWITCH_ATTR_DEFAULT_TRAP_GROUP:
            case SAI_SWITCH_ATTR_ECMP_HASH:
            case SAI_SWITCH_ATTR_LAG_HASH:
            case SAI_SWITCH_ATTR_RESTART_TYPE:
            case SAI_SWITCH_ATTR_MIN_PLANNED_RESTART_INTERVAL:
            case SAI_SWITCH_ATTR_NV_STORAGE_SIZE:
            case SAI_SWITCH_ATTR_MAX_ACL_ACTION_COUNT:
            case SAI_SWITCH_ATTR_ACL_CAPABILITY:

            // RW
            case SAI_SWITCH_ATTR_SWITCHING_MODE:
            case SAI_SWITCH_ATTR_BCAST_CPU_FLOOD_ENABLE:
            case SAI_SWITCH_ATTR_MCAST_CPU_FLOOD_ENABLE:
            case SAI_SWITCH_ATTR_SRC_MAC_ADDRESS:
            case SAI_SWITCH_ATTR_MAX_LEARNED_ADDRESSES:
            case SAI_SWITCH_ATTR_FDB_AGING_TIME:
            case SAI_SWITCH_ATTR_FDB_UNICAST_MISS_ACTION:
            case SAI_SWITCH_ATTR_FDB_BROADCAST_MISS_ACTION:
            case SAI_SWITCH_ATTR_FDB_MULTICAST_MISS_ACTION:
            case SAI_SWITCH_ATTR_ECMP_DEFAULT_HASH_ALGORITHM:
            case SAI_SWITCH_ATTR_ECMP_DEFAULT_HASH_SEED:
            case SAI_SWITCH_ATTR_ECMP_DEFAULT_SYMMETRIC_HASH:
            // case SAI_SWITCH_ATTR_ECMP_HASH_IPV4:
            // case SAI_SWITCH_ATTR_ECMP_HASH_IPV4_IN_IPV4:
            // case SAI_SWITCH_ATTR_ECMP_HASH_IPV6:
            case SAI_SWITCH_ATTR_LAG_DEFAULT_HASH_ALGORITHM:
            case SAI_SWITCH_ATTR_LAG_DEFAULT_HASH_SEED:
            case SAI_SWITCH_ATTR_LAG_DEFAULT_SYMMETRIC_HASH:
            // case SAI_SWITCH_ATTR_LAG_HASH_IPV4:
            // case SAI_SWITCH_ATTR_LAG_HASH_IPV4_IN_IPV4:
            // case SAI_SWITCH_ATTR_LAG_HASH_IPV6:
            case SAI_SWITCH_ATTR_COUNTER_REFRESH_INTERVAL:
            // case SAI_SWITCH_ATTR_QOS_DEFAULT_TC:
            //case SAI_SWITCH_ATTR_QOS_DOT1P_TO_TC_MAP:
            //case SAI_SWITCH_ATTR_QOS_DOT1P_TO_COLOR_MAP:
            //case SAI_SWITCH_ATTR_QOS_DSCP_TO_TC_MAP:
            //case SAI_SWITCH_ATTR_QOS_DSCP_TO_COLOR_MAP:
            //case SAI_SWITCH_ATTR_QOS_TC_TO_QUEUE_MAP:
            //case SAI_SWITCH_ATTR_QOS_TC_AND_COLOR_TO_DOT1P_MAP:
            //case SAI_SWITCH_ATTR_QOS_TC_AND_COLOR_TO_DSCP_MAP:
            // ok
                break;

            default:

                SWSS_LOG_ERROR("getting attribute id %d is not supported", attr->id);

                return SAI_STATUS_INVALID_PARAMETER;
        }
    }

    sai_status_t status = redis_generic_get(
            SAI_OBJECT_TYPE_SWITCH,
            (sai_object_id_t)0,
            attr_count,
            attr_list);

    if (status == SAI_STATUS_SUCCESS)
    {
        // TODO should we obtain this right away in switch init ? same for port ids ? and all default objects ?
        const sai_attribute_t* attr_cpu_port_id = redis_get_attribute_by_id(SAI_SWITCH_ATTR_CPU_PORT, attr_count, attr_list);

        // cpu port ID can be only obtained by sai GET switch API
        // and this port can't be removed from the switch

        if (attr_cpu_port_id != NULL)
        {
            sai_object_id_t cpu_port_id = attr_cpu_port_id->value.oid;

            if (local_cpu_port_id != SAI_NULL_OBJECT_ID)
            {
                if (local_cpu_port_id != cpu_port_id)
                {
                    // user requested to get cpu port id again
                    // just sanity check if id is different, then there is bug in code

                    SWSS_LOG_ERROR("previous cpu port id %llx, current cpu port id %llx", local_cpu_port_id, cpu_port_id);

                    return SAI_STATUS_FAILURE;
                }
            }

            local_cpu_port_id = cpu_port_id;

            SWSS_LOG_NOTICE("got cpu port ID %llx via get api", local_cpu_port_id);
        }

        const sai_attribute_t* attr_def_vr_id = redis_get_attribute_by_id(SAI_SWITCH_ATTR_DEFAULT_VIRTUAL_ROUTER_ID, attr_count, attr_list);

        // default virtual router ID can be only obtained by sai GET switch API
        // and this router can't be removed from the switch

        if (attr_def_vr_id != NULL)
        {
            sai_object_id_t vr_id = attr_def_vr_id->value.oid;

            if (local_default_virtual_router_id != SAI_NULL_OBJECT_ID)
            {
                if (local_default_virtual_router_id != vr_id)
                {
                    // user requested to get default virtual router id again
                    // just sanity check if id is different, then there is bug in code

                    SWSS_LOG_ERROR("previous default VR id %llx, current default VR id %llx", local_default_virtual_router_id, vr_id);

                    return SAI_STATUS_FAILURE;
                }
            }

            local_default_virtual_router_id = vr_id;

            SWSS_LOG_NOTICE("got default virtual router ID %llx via get api", local_default_virtual_router_id);
        }

        const sai_attribute_t* attr_port_list = redis_get_attribute_by_id(SAI_SWITCH_ATTR_PORT_LIST, attr_count, attr_list);

        if (attr_port_list != NULL)
        {
            sai_object_list_t port_list = attr_port_list->value.objlist;

            bool empty = local_ports_set.size() == 0;

            if (empty)
            {
                for (uint32_t i =0; i < port_list.count; ++i)
                {
                    local_ports_set.insert(port_list.list[i]);
                }

                SWSS_LOG_NOTICE("got %u ports via get api", port_list.count);
            }
            else
            {
                // port list is not empty, just a sanity checkH
                // if second get will return the same list

                for (uint32_t i = 0; i < port_list.count; ++i)
                {
                    sai_object_id_t port_id = port_list.list[i];

                    if (local_ports_set.find(port_id) == local_ports_set.end())
                    {
                        SWSS_LOG_ERROR("current port %llx was not found on previous list", port_id);

                        return SAI_STATUS_FAILURE;
                    }
                }
            }
        }
    }

    return status;
}

/**
 * @brief Switch method table retrieved with sai_api_query()
 */
const sai_switch_api_t redis_switch_api = {
    redis_initialize_switch,
    redis_shutdown_switch,
    redis_connect_switch,
    redis_disconnect_switch,
    redis_set_switch_attribute,
    redis_get_switch_attribute,
};
