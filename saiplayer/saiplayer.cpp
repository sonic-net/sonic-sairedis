#include <iostream>
#include <stdexcept>
#include <sstream>
#include <string>

#include <getopt.h>
#include <unistd.h>

#include "string.h"
extern "C" {
#include "sai.h"
}

#include "meta/saiserialize.h"
#include "meta/saiattributelist.h"
#include "swss/logger.h"
#include "swss/tokenize.h"
#include "sairedis.h"

std::map<std::string, std::string> profile_map;

const char *test_profile_get_value (
        _In_ sai_switch_profile_id_t profile_id,
        _In_ const char *variable)
{
    SWSS_LOG_ENTER();

    auto it = profile_map.find(variable);

    if (it == profile_map.end())
    {
        return NULL;
    }

    return it->second.c_str();
}

int test_profile_get_next_value (
        _In_ sai_switch_profile_id_t profile_id,
        _Out_ const char **variable,
        _Out_ const char **value)
{
    SWSS_LOG_ENTER();

    return -1;
}

const service_method_table_t test_services = {
    test_profile_get_value,
    test_profile_get_next_value
};

sai_acl_api_t                *sai_acl_api;
sai_buffer_api_t             *sai_buffer_api;
sai_fdb_api_t                *sai_fdb_api;
sai_hash_api_t               *sai_hash_api;
sai_hostif_api_t             *sai_hostif_api;
sai_lag_api_t                *sai_lag_api;
sai_mirror_api_t             *sai_mirror_api;
sai_neighbor_api_t           *sai_neighbor_api;
sai_next_hop_api_t           *sai_next_hop_api;
sai_next_hop_group_api_t     *sai_next_hop_group_api;
sai_policer_api_t            *sai_policer_api;
sai_port_api_t               *sai_port_api;
sai_qos_map_api_t            *sai_qos_map_api;
sai_queue_api_t              *sai_queue_api;
sai_route_api_t              *sai_route_api;
sai_router_interface_api_t   *sai_router_interface_api;
sai_samplepacket_api_t       *sai_samplepacket_api;
sai_scheduler_api_t          *sai_scheduler_api;
sai_scheduler_group_api_t    *sai_scheduler_group_api;
sai_stp_api_t                *sai_stp_api;
sai_switch_api_t             *sai_switch_api;
sai_tunnel_api_t             *sai_tunnel_api;
sai_udf_api_t                *sai_udf_api;
sai_virtual_router_api_t     *sai_router_api;
sai_vlan_api_t               *sai_vlan_api;
sai_wred_api_t               *sai_wred_api;

typedef sai_status_t (*create_fn)(
        _Out_ sai_object_id_t *stp_id,
        _In_  uint32_t attr_count,
        _In_  const sai_attribute_t *attr_list);

typedef sai_status_t (*remove_fn)(
        _In_ sai_object_id_t stp_id);

typedef sai_status_t (*set_attribute_fn)(
        _In_ sai_object_id_t object_id,
        _In_ const sai_attribute_t *attr);

typedef sai_status_t (*get_attribute_fn)(
        _In_ sai_object_id_t stp_id,
        _In_ uint32_t attr_count,
        _Inout_ sai_attribute_t *attr_list);

create_fn           common_create[SAI_OBJECT_TYPE_MAX];
remove_fn           common_remove[SAI_OBJECT_TYPE_MAX];
set_attribute_fn    common_set_attribute[SAI_OBJECT_TYPE_MAX];
get_attribute_fn    common_get_attribute[SAI_OBJECT_TYPE_MAX];

void initialize_common_api_pointers()
{
    SWSS_LOG_ENTER();

    common_create[SAI_OBJECT_TYPE_PORT] = NULL;
    common_create[SAI_OBJECT_TYPE_LAG] = (sai_lag_api) ? sai_lag_api->create_lag : NULL;
    common_create[SAI_OBJECT_TYPE_VIRTUAL_ROUTER] = (sai_router_api) ? sai_router_api->create_virtual_router : NULL;
    common_create[SAI_OBJECT_TYPE_NEXT_HOP] = (sai_next_hop_api) ? sai_next_hop_api->create_next_hop : NULL;
    common_create[SAI_OBJECT_TYPE_NEXT_HOP_GROUP] = (sai_next_hop_group_api) ? sai_next_hop_group_api->create_next_hop_group : NULL;
    common_create[SAI_OBJECT_TYPE_ROUTER_INTERFACE] = (sai_router_interface_api) ? sai_router_interface_api->create_router_interface : NULL;
    common_create[SAI_OBJECT_TYPE_ACL_TABLE] = (sai_acl_api) ? sai_acl_api->create_acl_table : NULL;
    common_create[SAI_OBJECT_TYPE_ACL_ENTRY] = (sai_acl_api) ? sai_acl_api->create_acl_entry : NULL;
    common_create[SAI_OBJECT_TYPE_ACL_COUNTER] = (sai_acl_api) ? sai_acl_api->create_acl_counter : NULL;
    common_create[SAI_OBJECT_TYPE_HOST_INTERFACE] = (sai_hostif_api) ? sai_hostif_api->create_hostif : NULL;
    common_create[SAI_OBJECT_TYPE_TRAP_GROUP] = (sai_hostif_api) ? sai_hostif_api->create_hostif_trap_group : NULL;
    common_create[SAI_OBJECT_TYPE_ACL_TABLE_GROUP] = NULL;
    common_create[SAI_OBJECT_TYPE_POLICER] = (sai_policer_api) ? sai_policer_api->create_policer : NULL;
    common_create[SAI_OBJECT_TYPE_WRED] = (sai_wred_api) ? sai_wred_api->create_wred_profile : NULL;
    common_create[SAI_OBJECT_TYPE_QOS_MAPS] = (sai_qos_map_api) ? sai_qos_map_api->create_qos_map : NULL;
    common_create[SAI_OBJECT_TYPE_QUEUE] = NULL;
    common_create[SAI_OBJECT_TYPE_SCHEDULER] = (sai_scheduler_api) ? sai_scheduler_api->create_scheduler_profile : NULL;
    common_create[SAI_OBJECT_TYPE_SCHEDULER_GROUP] = (sai_scheduler_group_api) ? sai_scheduler_group_api->create_scheduler_group : NULL;
    common_create[SAI_OBJECT_TYPE_BUFFER_POOL] = (sai_buffer_api) ? sai_buffer_api->create_buffer_pool : NULL;
    common_create[SAI_OBJECT_TYPE_BUFFER_PROFILE] = (sai_buffer_api) ? sai_buffer_api->create_buffer_profile : NULL;
    common_create[SAI_OBJECT_TYPE_PRIORITY_GROUP] = NULL;
    common_create[SAI_OBJECT_TYPE_LAG_MEMBER] = (sai_lag_api) ? sai_lag_api->create_lag_member : NULL;
    common_create[SAI_OBJECT_TYPE_VLAN_MEMBER] = (sai_vlan_api) ? sai_vlan_api->create_vlan_member : NULL;
    common_create[SAI_OBJECT_TYPE_TUNNEL] = (sai_tunnel_api) ? sai_tunnel_api->create_tunnel : NULL;
    common_create[SAI_OBJECT_TYPE_TUNNEL_TABLE_ENTRY] = (sai_tunnel_api) ? sai_tunnel_api->create_tunnel_term_table_entry : NULL;

    common_remove[SAI_OBJECT_TYPE_PORT] = NULL;
    common_remove[SAI_OBJECT_TYPE_LAG] = (sai_lag_api) ? sai_lag_api->remove_lag : NULL;
    common_remove[SAI_OBJECT_TYPE_VIRTUAL_ROUTER] = (sai_router_api) ? sai_router_api->remove_virtual_router : NULL;
    common_remove[SAI_OBJECT_TYPE_NEXT_HOP] = (sai_next_hop_api) ? sai_next_hop_api->remove_next_hop : NULL;
    common_remove[SAI_OBJECT_TYPE_NEXT_HOP_GROUP] = (sai_next_hop_group_api) ? sai_next_hop_group_api->remove_next_hop_group : NULL;
    common_remove[SAI_OBJECT_TYPE_ROUTER_INTERFACE] = (sai_router_interface_api) ? sai_router_interface_api->remove_router_interface : NULL;
    common_remove[SAI_OBJECT_TYPE_ACL_TABLE] = (sai_acl_api) ? sai_acl_api->delete_acl_table : NULL;
    common_remove[SAI_OBJECT_TYPE_ACL_ENTRY] = (sai_acl_api) ? sai_acl_api->delete_acl_entry : NULL;
    common_remove[SAI_OBJECT_TYPE_ACL_COUNTER] = (sai_acl_api) ? sai_acl_api->delete_acl_counter : NULL;
    common_remove[SAI_OBJECT_TYPE_HOST_INTERFACE] = (sai_hostif_api) ? sai_hostif_api->remove_hostif : NULL;
    common_remove[SAI_OBJECT_TYPE_TRAP_GROUP] = (sai_hostif_api) ? sai_hostif_api->remove_hostif_trap_group : NULL;
    common_remove[SAI_OBJECT_TYPE_ACL_TABLE_GROUP] = NULL;
    common_remove[SAI_OBJECT_TYPE_POLICER] = (sai_policer_api) ? sai_policer_api->remove_policer : NULL;
    common_remove[SAI_OBJECT_TYPE_WRED] = (sai_wred_api) ? sai_wred_api->remove_wred_profile : NULL;
    common_remove[SAI_OBJECT_TYPE_QOS_MAPS] = (sai_qos_map_api) ? sai_qos_map_api->remove_qos_map : NULL;
    common_remove[SAI_OBJECT_TYPE_QUEUE] = NULL;
    common_remove[SAI_OBJECT_TYPE_SCHEDULER] = (sai_scheduler_api) ? sai_scheduler_api->remove_scheduler_profile : NULL;
    common_remove[SAI_OBJECT_TYPE_SCHEDULER_GROUP] = (sai_scheduler_group_api) ? sai_scheduler_group_api->remove_scheduler_group : NULL;
    common_remove[SAI_OBJECT_TYPE_BUFFER_POOL] = (sai_buffer_api) ? sai_buffer_api->remove_buffer_pool : NULL;
    common_remove[SAI_OBJECT_TYPE_BUFFER_PROFILE] = (sai_buffer_api) ? sai_buffer_api->remove_buffer_profile : NULL;
    common_remove[SAI_OBJECT_TYPE_PRIORITY_GROUP] = NULL;
    common_remove[SAI_OBJECT_TYPE_LAG_MEMBER] = (sai_lag_api) ? sai_lag_api->remove_lag_member : NULL;
    common_remove[SAI_OBJECT_TYPE_VLAN_MEMBER] = (sai_vlan_api) ? sai_vlan_api->remove_vlan_member : NULL;
    common_remove[SAI_OBJECT_TYPE_TUNNEL] = (sai_tunnel_api) ? sai_tunnel_api->remove_tunnel : NULL;
    common_remove[SAI_OBJECT_TYPE_TUNNEL_TABLE_ENTRY] = (sai_tunnel_api) ? sai_tunnel_api->remove_tunnel_term_table_entry : NULL;

    common_set_attribute[SAI_OBJECT_TYPE_PORT] = (sai_port_api) ? sai_port_api->set_port_attribute : NULL;
    common_set_attribute[SAI_OBJECT_TYPE_LAG] = (sai_lag_api) ? sai_lag_api->set_lag_attribute : NULL;
    common_set_attribute[SAI_OBJECT_TYPE_VIRTUAL_ROUTER] = (sai_router_api) ? sai_router_api->set_virtual_router_attribute : NULL;
    common_set_attribute[SAI_OBJECT_TYPE_NEXT_HOP] = (sai_next_hop_api) ? sai_next_hop_api->set_next_hop_attribute : NULL;
    common_set_attribute[SAI_OBJECT_TYPE_NEXT_HOP_GROUP] = (sai_next_hop_group_api) ? sai_next_hop_group_api->set_next_hop_group_attribute : NULL;
    common_set_attribute[SAI_OBJECT_TYPE_ROUTER_INTERFACE] = (sai_router_interface_api) ? sai_router_interface_api->set_router_interface_attribute : NULL;
    common_set_attribute[SAI_OBJECT_TYPE_ACL_TABLE] = (sai_acl_api) ? sai_acl_api->set_acl_table_attribute : NULL;
    common_set_attribute[SAI_OBJECT_TYPE_ACL_ENTRY] = (sai_acl_api) ? sai_acl_api->set_acl_entry_attribute : NULL;
    common_set_attribute[SAI_OBJECT_TYPE_ACL_COUNTER] = (sai_acl_api) ? sai_acl_api->set_acl_counter_attribute : NULL;
    common_set_attribute[SAI_OBJECT_TYPE_HOST_INTERFACE] = (sai_hostif_api) ? sai_hostif_api->set_hostif_attribute : NULL;
    common_set_attribute[SAI_OBJECT_TYPE_TRAP_GROUP] = (sai_hostif_api) ? sai_hostif_api->set_trap_group_attribute : NULL;
    common_set_attribute[SAI_OBJECT_TYPE_ACL_TABLE_GROUP] = NULL;
    common_set_attribute[SAI_OBJECT_TYPE_POLICER] = (sai_policer_api) ? sai_policer_api->set_policer_attribute : NULL;
    common_set_attribute[SAI_OBJECT_TYPE_WRED] = (sai_wred_api) ? sai_wred_api->set_wred_attribute : NULL;
    common_set_attribute[SAI_OBJECT_TYPE_QOS_MAPS] = (sai_qos_map_api) ? sai_qos_map_api->set_qos_map_attribute : NULL;
    common_set_attribute[SAI_OBJECT_TYPE_QUEUE] = (sai_queue_api) ? sai_queue_api->set_queue_attribute : NULL;
    common_set_attribute[SAI_OBJECT_TYPE_SCHEDULER] = (sai_scheduler_api) ? sai_scheduler_api->set_scheduler_attribute : NULL;
    common_set_attribute[SAI_OBJECT_TYPE_SCHEDULER_GROUP] = (sai_scheduler_group_api) ? sai_scheduler_group_api->set_scheduler_group_attribute : NULL;
    common_set_attribute[SAI_OBJECT_TYPE_BUFFER_POOL] = (sai_buffer_api) ? sai_buffer_api->set_buffer_pool_attr : NULL;
    common_set_attribute[SAI_OBJECT_TYPE_BUFFER_PROFILE] = (sai_buffer_api) ? sai_buffer_api->set_buffer_profile_attr : NULL;
    common_set_attribute[SAI_OBJECT_TYPE_PRIORITY_GROUP] = (sai_buffer_api) ? sai_buffer_api->set_ingress_priority_group_attr : NULL;
    common_set_attribute[SAI_OBJECT_TYPE_LAG_MEMBER] = (sai_lag_api) ? sai_lag_api->set_lag_member_attribute : NULL;
    common_set_attribute[SAI_OBJECT_TYPE_VLAN_MEMBER] = (sai_vlan_api) ? sai_vlan_api->set_vlan_member_attribute : NULL;
    common_set_attribute[SAI_OBJECT_TYPE_TUNNEL] = (sai_tunnel_api) ? sai_tunnel_api->set_tunnel_attribute : NULL;
    common_set_attribute[SAI_OBJECT_TYPE_TUNNEL_TABLE_ENTRY] = (sai_tunnel_api) ? sai_tunnel_api->set_tunnel_term_table_entry_attribute : NULL;

    common_get_attribute[SAI_OBJECT_TYPE_PORT] = (sai_port_api) ? sai_port_api->get_port_attribute : NULL;
    common_get_attribute[SAI_OBJECT_TYPE_LAG] = (sai_lag_api) ? sai_lag_api->get_lag_attribute : NULL;
    common_get_attribute[SAI_OBJECT_TYPE_VIRTUAL_ROUTER] = (sai_router_api) ? sai_router_api->get_virtual_router_attribute : NULL;
    common_get_attribute[SAI_OBJECT_TYPE_NEXT_HOP] = (sai_next_hop_api) ? sai_next_hop_api->get_next_hop_attribute : NULL;
    common_get_attribute[SAI_OBJECT_TYPE_NEXT_HOP_GROUP] = (sai_next_hop_group_api) ? sai_next_hop_group_api->get_next_hop_group_attribute : NULL;
    common_get_attribute[SAI_OBJECT_TYPE_ROUTER_INTERFACE] = (sai_router_interface_api) ? sai_router_interface_api->get_router_interface_attribute : NULL;
    common_get_attribute[SAI_OBJECT_TYPE_ACL_TABLE] = (sai_acl_api) ? sai_acl_api->get_acl_table_attribute : NULL;
    common_get_attribute[SAI_OBJECT_TYPE_ACL_ENTRY] = (sai_acl_api) ? sai_acl_api->get_acl_entry_attribute : NULL;
    common_get_attribute[SAI_OBJECT_TYPE_ACL_COUNTER] = (sai_acl_api) ? sai_acl_api->get_acl_counter_attribute : NULL;
    common_get_attribute[SAI_OBJECT_TYPE_HOST_INTERFACE] = (sai_hostif_api) ? sai_hostif_api->get_hostif_attribute : NULL;
    common_get_attribute[SAI_OBJECT_TYPE_TRAP_GROUP] = (sai_hostif_api) ? sai_hostif_api->get_trap_group_attribute : NULL;
    common_get_attribute[SAI_OBJECT_TYPE_ACL_TABLE_GROUP] = NULL;
    common_get_attribute[SAI_OBJECT_TYPE_POLICER] = (sai_policer_api) ? sai_policer_api->get_policer_attribute : NULL;
    common_get_attribute[SAI_OBJECT_TYPE_WRED] = (sai_wred_api) ? sai_wred_api->get_wred_attribute : NULL;
    common_get_attribute[SAI_OBJECT_TYPE_QOS_MAPS] = (sai_qos_map_api) ? sai_qos_map_api->get_qos_map_attribute : NULL;
    common_get_attribute[SAI_OBJECT_TYPE_QUEUE] = (sai_queue_api) ? sai_queue_api->get_queue_attribute : NULL;
    common_get_attribute[SAI_OBJECT_TYPE_SCHEDULER] = (sai_scheduler_api) ? sai_scheduler_api->get_scheduler_attribute : NULL;
    common_get_attribute[SAI_OBJECT_TYPE_SCHEDULER_GROUP] = (sai_scheduler_group_api) ? sai_scheduler_group_api->get_scheduler_group_attribute : NULL;
    common_get_attribute[SAI_OBJECT_TYPE_BUFFER_POOL] = (sai_buffer_api) ? sai_buffer_api->get_buffer_pool_attr : NULL;
    common_get_attribute[SAI_OBJECT_TYPE_BUFFER_PROFILE] = (sai_buffer_api) ? sai_buffer_api->get_buffer_profile_attr : NULL;
    common_get_attribute[SAI_OBJECT_TYPE_PRIORITY_GROUP] = (sai_buffer_api) ? sai_buffer_api->get_ingress_priority_group_attr : NULL;
    common_get_attribute[SAI_OBJECT_TYPE_LAG_MEMBER] = (sai_lag_api) ? sai_lag_api->get_lag_member_attribute : NULL;
    common_get_attribute[SAI_OBJECT_TYPE_VLAN_MEMBER] = (sai_vlan_api) ? sai_vlan_api->get_vlan_member_attribute : NULL;
    common_get_attribute[SAI_OBJECT_TYPE_TUNNEL] = (sai_tunnel_api) ? sai_tunnel_api->get_tunnel_attribute : NULL;
    common_get_attribute[SAI_OBJECT_TYPE_TUNNEL_TABLE_ENTRY] = (sai_tunnel_api) ? sai_tunnel_api->get_tunnel_term_table_entry_attribute : NULL;
}

void init_sai_api()
{
    SWSS_LOG_ENTER();

    sai_api_query(SAI_API_ACL,                  (void**)&sai_acl_api);
    sai_api_query(SAI_API_BUFFERS,              (void**)&sai_buffer_api);
    sai_api_query(SAI_API_FDB,                  (void**)&sai_fdb_api);
    sai_api_query(SAI_API_HASH,                 (void**)&sai_hash_api);
    sai_api_query(SAI_API_HOST_INTERFACE,       (void**)&sai_hostif_api);
    sai_api_query(SAI_API_LAG,                  (void**)&sai_lag_api);
    sai_api_query(SAI_API_MIRROR,               (void**)&sai_mirror_api);
    sai_api_query(SAI_API_NEIGHBOR,             (void**)&sai_neighbor_api);
    sai_api_query(SAI_API_NEXT_HOP,             (void**)&sai_next_hop_api);
    sai_api_query(SAI_API_NEXT_HOP_GROUP,       (void**)&sai_next_hop_group_api);
    sai_api_query(SAI_API_POLICER,              (void**)&sai_policer_api);
    sai_api_query(SAI_API_PORT,                 (void**)&sai_port_api);
    sai_api_query(SAI_API_QOS_MAPS,             (void**)&sai_qos_map_api);
    sai_api_query(SAI_API_QUEUE,                (void**)&sai_queue_api);
    sai_api_query(SAI_API_ROUTE,                (void**)&sai_route_api);
    sai_api_query(SAI_API_ROUTER_INTERFACE,     (void**)&sai_router_interface_api);
    sai_api_query(SAI_API_SAMPLEPACKET,         (void**)&sai_samplepacket_api);
    sai_api_query(SAI_API_SCHEDULER,            (void**)&sai_scheduler_api);
    sai_api_query(SAI_API_SCHEDULER_GROUP,      (void**)&sai_scheduler_group_api);
    sai_api_query(SAI_API_STP,                  (void**)&sai_stp_api);
    sai_api_query(SAI_API_SWITCH,               (void**)&sai_switch_api);
    sai_api_query(SAI_API_TUNNEL,               (void**)&sai_tunnel_api);
    sai_api_query(SAI_API_UDF,                  (void**)&sai_udf_api);
    sai_api_query(SAI_API_VIRTUAL_ROUTER,       (void**)&sai_router_api);
    sai_api_query(SAI_API_VLAN,                 (void**)&sai_vlan_api);
    sai_api_query(SAI_API_WRED,                 (void**)&sai_wred_api);
}

void on_switch_state_change(
        _In_ sai_switch_oper_status_t switch_oper_status)
{
    SWSS_LOG_ENTER();
}

void on_fdb_event(
        _In_ uint32_t count,
        _In_ sai_fdb_event_notification_data_t *data)
{
    SWSS_LOG_ENTER();
}

void on_port_state_change(
        _In_ uint32_t count,
        _In_ sai_port_oper_status_notification_t *data)
{
    SWSS_LOG_ENTER();
}

void on_port_event(
        _In_ uint32_t count,
        _In_ sai_port_event_notification_t *data)
{
    SWSS_LOG_ENTER();
}

void on_switch_shutdown_request() __attribute__ ((noreturn));
void on_switch_shutdown_request()
{
    SWSS_LOG_ENTER();

    SWSS_LOG_ERROR("got shutdown request, syncd failed!");
    exit(EXIT_FAILURE);
}

void on_packet_event(
        _In_ const void *buffer,
        _In_ sai_size_t buffer_size,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();
}

sai_switch_notification_t switch_notifications
{
    on_switch_state_change,
        on_fdb_event,
        on_port_state_change,
        on_port_event,
        on_switch_shutdown_request,
        on_packet_event
};

#define EXIT_ON_ERROR(x)\
{\
    sai_status_t s = (x);\
    if (s != SAI_STATUS_SUCCESS)\
    {\
        SWSS_LOG_ERROR("fail status %d", s);\
        exit(EXIT_FAILURE);\
    }\
}

// to recorded
std::map<sai_object_id_t,sai_object_id_t> local_to_redis;
std::map<sai_object_id_t,sai_object_id_t> redis_to_local;

sai_object_id_t translate_local_to_redis(
        _In_ sai_object_id_t rid)
{
    SWSS_LOG_ENTER();

    SWSS_LOG_DEBUG("translating local 0x%lx", rid);

    auto it = local_to_redis.find(rid);

    if (it == local_to_redis.end())
    {
        SWSS_LOG_ERROR("failed to translate local 0x%lx", rid);
        exit(EXIT_FAILURE);
    }

    return it->second;
}

    template <typename T>
void translate_local_to_redis(
        _In_ T &element)
{
    SWSS_LOG_ENTER();

    for (uint32_t i = 0; i < element.count; i++)
    {
        element.list[i] = translate_local_to_redis(element.list[i]);
    }
}

void translate_local_to_redis(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    for (uint32_t i = 0; i < attr_count; i++)
    {
        sai_attribute_t &attr = attr_list[i];

        auto meta = get_attribute_metadata(object_type, attr.id);

        if (meta == NULL)
        {
            SWSS_LOG_ERROR("unable to get metadata for object type %x, attribute %x", object_type, attr.id);
            exit(EXIT_FAILURE);
        }

        switch (meta->serializationtype)
        {
            case SAI_SERIALIZATION_TYPE_OBJECT_ID:
                attr.value.oid = translate_local_to_redis(attr.value.oid);
                break;

            case SAI_SERIALIZATION_TYPE_OBJECT_LIST:
                translate_local_to_redis(attr.value.objlist);
                break;

            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_OBJECT_ID:
                attr.value.aclfield.data.oid = translate_local_to_redis(attr.value.aclfield.data.oid);
                break;

            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_OBJECT_LIST:
                translate_local_to_redis(attr.value.aclfield.data.objlist);
                break;

            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_ID:
                attr.value.aclaction.parameter.oid = translate_local_to_redis(attr.value.aclaction.parameter.oid);
                break;

            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_LIST:
                translate_local_to_redis(attr.value.aclaction.parameter.objlist);
                break;

            default:
                break;
        }
    }
}

sai_object_type_t deserialize_object_type(const std::string& s)
{
    sai_object_type_t object_type;

    sai_deserialize_object_type(s, object_type);

    return object_type;
}

const std::vector<swss::FieldValueTuple> get_values(const std::vector<std::string>& items)
{
    std::vector<swss::FieldValueTuple> values;

    // timestamp|action|objecttype:objectid|attrid=value,...
    for (size_t i = 3; i <items.size(); ++i)
    {
        const std::string& item = items[i];

        auto start = item.find_first_of("=");

        auto field = item.substr(0, start);
        auto value = item.substr(start + 1);

        swss::FieldValueTuple entry(field, value);

        values.push_back(entry);
    }

    return values;
}

#define CHECK_LIST(x)\
    if (attr.x.count != get_attr.x.count)\
{ SWSS_LOG_ERROR("get response list count not match recording"); exit(EXIT_FAILURE); }

void match_list_lengths(
        sai_object_type_t object_type,
        uint32_t get_attr_count,
        sai_attribute_t* get_attr_list,
        uint32_t attr_count,
        sai_attribute_t* attr_list)
{
    SWSS_LOG_ENTER();

    if (get_attr_count != attr_count)
    {
        SWSS_LOG_ERROR("list number don't match %u != %u", get_attr_count, attr_count);
        exit(EXIT_FAILURE);
    }

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        sai_attribute_t &get_attr = get_attr_list[i];
        sai_attribute_t &attr = attr_list[i];

        auto meta = get_attribute_metadata(object_type, attr.id);

        if (meta == NULL)
        {
            SWSS_LOG_ERROR("unable to get metadata for object type %x, attribute %x", object_type, attr.id);
            exit(EXIT_FAILURE);
        }

        switch (meta->serializationtype)
        {
            case SAI_SERIALIZATION_TYPE_OBJECT_LIST:
                CHECK_LIST(value.objlist);
                break;

            case SAI_SERIALIZATION_TYPE_UINT8_LIST:
                CHECK_LIST(value.u8list);
                break;

            case SAI_SERIALIZATION_TYPE_INT8_LIST:
                CHECK_LIST(value.s8list);
                break;

            case SAI_SERIALIZATION_TYPE_UINT16_LIST:
                CHECK_LIST(value.u16list);
                break;

            case SAI_SERIALIZATION_TYPE_INT16_LIST:
                CHECK_LIST(value.s16list);
                break;

            case SAI_SERIALIZATION_TYPE_UINT32_LIST:
                CHECK_LIST(value.u32list);
                break;

            case SAI_SERIALIZATION_TYPE_INT32_LIST:
                CHECK_LIST(value.s32list);
                break;

            case SAI_SERIALIZATION_TYPE_VLAN_LIST:
                CHECK_LIST(value.vlanlist);
                break;

            case SAI_SERIALIZATION_TYPE_QOS_MAP_LIST:
                CHECK_LIST(value.qosmap);
                break;

            case SAI_SERIALIZATION_TYPE_TUNNEL_MAP_LIST:
                CHECK_LIST(value.tunnelmap);
                break;

            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_OBJECT_LIST:
                CHECK_LIST(value.aclfield.data.objlist);
                break;

            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_UINT8_LIST:
                CHECK_LIST(value.aclfield.data.u8list);
                CHECK_LIST(value.aclfield.mask.u8list);
                break;

            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_LIST:
                CHECK_LIST(value.aclaction.parameter.objlist);
                break;

            default:
                break;
        }
    }
}

void match_redis_with_rec(
        sai_object_id_t get_oid,
        sai_object_id_t oid)
{
    SWSS_LOG_ENTER();

    auto it = redis_to_local.find(get_oid);

    if (it == redis_to_local.end())
    {
        redis_to_local[get_oid] = oid;
        local_to_redis[oid] = get_oid;
    }

    if (oid != redis_to_local[get_oid])
    {
        SWSS_LOG_ERROR("match failed, oid order is mismatch :( oid 0x%lx get_oid 0x%lx second 0x%lx",
                oid,
                get_oid,
                redis_to_local[get_oid]);

        exit(EXIT_FAILURE);
    }

    SWSS_LOG_DEBUG("map size: %zu", local_to_redis.size());
}

void match_redis_with_rec(
        sai_object_list_t get_objlist,
        sai_object_list_t objlist)
{
    SWSS_LOG_ENTER();

    for (uint32_t i = 0 ; i < get_objlist.count; ++i)
    {
        match_redis_with_rec(get_objlist.list[i], objlist.list[i]);
    }
}

void match_redis_with_rec(
        sai_object_type_t object_type,
        uint32_t get_attr_count,
        sai_attribute_t* get_attr_list,
        uint32_t attr_count,
        sai_attribute_t* attr_list)
{
    SWSS_LOG_ENTER();

    if (get_attr_count != attr_count)
    {
        SWSS_LOG_ERROR("list number don't match %u != %u", get_attr_count, attr_count);
        exit(EXIT_FAILURE);
    }

    for (uint32_t i = 0; i < attr_count; ++i)
    {
        sai_attribute_t &get_attr = get_attr_list[i];
        sai_attribute_t &attr = attr_list[i];

        auto meta = get_attribute_metadata(object_type, attr.id);

        if (meta == NULL)
        {
            SWSS_LOG_ERROR("unable to get metadata for object type %x, attribute %x", object_type, attr.id);
            exit(EXIT_FAILURE);
        }

        switch (meta->serializationtype)
        {
            case SAI_SERIALIZATION_TYPE_OBJECT_ID:
                match_redis_with_rec(get_attr.value.oid, attr.value.oid);
                break;

            case SAI_SERIALIZATION_TYPE_OBJECT_LIST:
                match_redis_with_rec(get_attr.value.objlist, attr.value.objlist);
                break;

            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_OBJECT_ID:
                match_redis_with_rec(get_attr.value.aclfield.data.oid, attr.value.aclfield.data.oid);
                break;

            case SAI_SERIALIZATION_TYPE_ACL_FIELD_DATA_OBJECT_LIST:
                match_redis_with_rec(get_attr.value.aclfield.data.objlist, attr.value.aclfield.data.objlist);
                break;

            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_ID:
                match_redis_with_rec(get_attr.value.aclaction.parameter.oid, attr.value.aclaction.parameter.oid);
                break;

            case SAI_SERIALIZATION_TYPE_ACL_ACTION_DATA_OBJECT_LIST:
                match_redis_with_rec(get_attr.value.aclaction.parameter.objlist, attr.value.aclaction.parameter.objlist);
                break;

            default:
                break;
        }
    }
}

sai_status_t handle_fdb(
        _In_ const std::string &str_object_id,
        _In_ sai_common_api_t api,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_fdb_entry_t fdb_entry;
    sai_deserialize_fdb_entry(str_object_id, fdb_entry);

    switch (api)
    {
        case SAI_COMMON_API_CREATE:
            return sai_fdb_api->create_fdb_entry(&fdb_entry, attr_count, attr_list);

        case SAI_COMMON_API_REMOVE:
            return sai_fdb_api->remove_fdb_entry(&fdb_entry);

        case SAI_COMMON_API_SET:
            return sai_fdb_api->set_fdb_entry_attribute(&fdb_entry, attr_list);

        case SAI_COMMON_API_GET:
            return sai_fdb_api->get_fdb_entry_attribute(&fdb_entry, attr_count, attr_list);

        default:
            SWSS_LOG_ERROR("fdb other apis not implemented");
            exit(EXIT_FAILURE);
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t handle_neighbor(
        _In_ const std::string &str_object_id,
        _In_ sai_common_api_t api,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_neighbor_entry_t neighbor_entry;
    sai_deserialize_neighbor_entry(str_object_id, neighbor_entry);

    neighbor_entry.rif_id = translate_local_to_redis(neighbor_entry.rif_id);

    switch(api)
    {
        case SAI_COMMON_API_CREATE:
            return sai_neighbor_api->create_neighbor_entry(&neighbor_entry, attr_count, attr_list);

        case SAI_COMMON_API_REMOVE:
            return sai_neighbor_api->remove_neighbor_entry(&neighbor_entry);

        case SAI_COMMON_API_SET:
            return sai_neighbor_api->set_neighbor_attribute(&neighbor_entry, attr_list);

        case SAI_COMMON_API_GET:
            return sai_neighbor_api->get_neighbor_attribute(&neighbor_entry, attr_count, attr_list);

        default:
            SWSS_LOG_ERROR("neighbor other apis not implemented");
            exit(EXIT_FAILURE);
    }
}

sai_status_t handle_route(
        _In_ const std::string &str_object_id,
        _In_ sai_common_api_t api,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_unicast_route_entry_t route_entry;
    sai_deserialize_route_entry(str_object_id, route_entry);

    route_entry.vr_id = translate_local_to_redis(route_entry.vr_id);

    SWSS_LOG_DEBUG("route: %s", str_object_id.c_str());

    switch(api)
    {
        case SAI_COMMON_API_CREATE:
            return sai_route_api->create_route(&route_entry, attr_count, attr_list);

        case SAI_COMMON_API_REMOVE:
            return sai_route_api->remove_route(&route_entry);

        case SAI_COMMON_API_SET:
            return sai_route_api->set_route_attribute(&route_entry, attr_list);

        case SAI_COMMON_API_GET:
            return sai_route_api->get_route_attribute(&route_entry, attr_count, attr_list);

        default:
            SWSS_LOG_ERROR("route other apis not implemented");
            exit(EXIT_FAILURE);
    }
}

sai_status_t handle_switch(
        _In_ const std::string &str_object_id,
        _In_ sai_common_api_t api,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    switch(api)
    {
        case SAI_COMMON_API_CREATE:
            return SAI_STATUS_NOT_SUPPORTED;

        case SAI_COMMON_API_REMOVE:
            return SAI_STATUS_NOT_SUPPORTED;

        case SAI_COMMON_API_SET:
            return sai_switch_api->set_switch_attribute(attr_list);

        case SAI_COMMON_API_GET:
            return sai_switch_api->get_switch_attribute(attr_count, attr_list);

        default:
            SWSS_LOG_ERROR("switch other apis not implemented");
            exit(EXIT_FAILURE);
    }
}

sai_status_t handle_vlan(
        _In_ const std::string &str_object_id,
        _In_ sai_common_api_t api,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_vlan_id_t vlan_id;
    sai_deserialize_vlan_id(str_object_id, vlan_id);

    switch(api)
    {
        case SAI_COMMON_API_CREATE:
            return sai_vlan_api->create_vlan(vlan_id);

        case SAI_COMMON_API_REMOVE:
            return sai_vlan_api->remove_vlan(vlan_id);

        case SAI_COMMON_API_SET:
            return sai_vlan_api->set_vlan_attribute(vlan_id, attr_list);

        case SAI_COMMON_API_GET:
            return sai_vlan_api->get_vlan_attribute(vlan_id, attr_count, attr_list);

        default:
            SWSS_LOG_ERROR("vlan other apis not implemented");
            exit(EXIT_FAILURE);
    }
}

sai_status_t handle_trap(
        _In_ const std::string &str_object_id,
        _In_ sai_common_api_t api,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_hostif_trap_id_t trap_id;
    sai_deserialize_hostif_trap_id(str_object_id, trap_id);

    switch(api)
    {
        case SAI_COMMON_API_CREATE:
            return SAI_STATUS_NOT_SUPPORTED;

        case SAI_COMMON_API_REMOVE:
            return SAI_STATUS_NOT_SUPPORTED;

        case SAI_COMMON_API_SET:
            return sai_hostif_api->set_trap_attribute(trap_id, attr_list);

        case SAI_COMMON_API_GET:
            return sai_hostif_api->get_trap_attribute(trap_id, attr_count, attr_list);

        default:
            SWSS_LOG_ERROR("trap other apis not implemented");
            exit(EXIT_FAILURE);
    }
}

sai_status_t handle_generic(
        _In_ sai_object_type_t object_type,
        _In_ const std::string &str_object_id,
        _In_ sai_common_api_t api,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    sai_object_id_t local_id;
    sai_deserialize_object_id(str_object_id, local_id);

    SWSS_LOG_DEBUG("common generic api: %d", api);

    switch (api)
    {
        case SAI_COMMON_API_CREATE:
            {
                SWSS_LOG_DEBUG("generic create for object type %x", object_type);

                create_fn create = common_create[object_type];

                if (create == NULL)
                {
                    SWSS_LOG_ERROR("create function is not defined for object type %x", object_type);
                    exit(EXIT_FAILURE);
                }

                sai_object_id_t rid;
                sai_status_t status = create(&rid, attr_count, attr_list);

                if (status == SAI_STATUS_SUCCESS)
                {
                    match_redis_with_rec(rid, local_id);

                    SWSS_LOG_INFO("saved VID 0x%lx to RID 0x%lx", local_id, rid);
                }
                else
                {
                    SWSS_LOG_ERROR("failed to create %d", status);
                }

                return status;
            }

        case SAI_COMMON_API_REMOVE:
            {
                SWSS_LOG_DEBUG("generic remove for object type %x", object_type);

                remove_fn remove = common_remove[object_type];

                if (remove == NULL)
                {
                    SWSS_LOG_ERROR("remove function is not defined for object type %x", object_type);
                    exit(EXIT_FAILURE);
                }

                sai_object_id_t rid = translate_local_to_redis(local_id);

                return remove(rid);
            }

        case SAI_COMMON_API_SET:
            {
                SWSS_LOG_DEBUG("generic set for object type %x", object_type);

                set_attribute_fn set = common_set_attribute[object_type];

                if (set == NULL)
                {
                    SWSS_LOG_ERROR("set function is not defined for object type %x", object_type);
                    exit(EXIT_FAILURE);
                }

                sai_object_id_t rid = translate_local_to_redis(local_id);

                return set(rid, attr_list);
            }

        case SAI_COMMON_API_GET:
            {
                SWSS_LOG_DEBUG("generic get for object type %x", object_type);

                get_attribute_fn get = common_get_attribute[object_type];

                if (get == NULL)
                {
                    SWSS_LOG_ERROR("get function is not defined for object type %x", object_type);
                    exit(EXIT_FAILURE);
                }

                sai_object_id_t rid = translate_local_to_redis(local_id);

                return get(rid, attr_count, attr_list);
            }

        default:
            SWSS_LOG_ERROR("generic other apis not implemented");
            exit(EXIT_FAILURE);
    }
}

void handle_get_response(
        sai_object_type_t object_type,
        uint32_t get_attr_count,
        sai_attribute_t* get_attr_list,
        const std::string& response)
{
    SWSS_LOG_ENTER();

    //std::cout << "processing " << response << std::endl;

    // timestamp|action|objecttype:objectid|attrid=value,...
    auto v = swss::tokenize(response, '|');

    auto values = get_values(v);

    SaiAttributeList list(object_type, values, false);

    sai_attribute_t *attr_list = list.get_attr_list();
    uint32_t attr_count = list.get_attr_count();

    match_list_lengths(object_type, get_attr_count, get_attr_list, attr_count, attr_list);

    SWSS_LOG_DEBUG("list match");

    match_redis_with_rec(object_type, get_attr_count, get_attr_list, attr_count, attr_list);

    // TODO primitive values are not matched (recording vs switch/vs), we can add that check
}

void performSleep(const std::string& line)
{
    SWSS_LOG_ENTER();

    // timestamp|action|sleeptime
    auto v = swss::tokenize(line, '|');

    if (v.size() < 3)
    {
        SWSS_LOG_ERROR("invalid line %s", line.c_str());
        exit(EXIT_FAILURE);
    }

    uint32_t useconds;
    sai_deserialize_number(v[2], useconds);

    if (useconds > 0)
    {
        useconds *= 1000; // 1ms resolution is enough for sleep

        SWSS_LOG_NOTICE("usleep(%u)", useconds);
        usleep(useconds);
    }
}

bool g_notifySyncd = true;

void performNotifySyncd(const std::string& request, const std::string response)
{
    SWSS_LOG_ENTER();

    // timestamp|action|data
    auto r = swss::tokenize(request, '|');
    auto R = swss::tokenize(response, '|');

    if (r[1] != "a" || R[1] != "A")
    {
        SWSS_LOG_ERROR("invalid syncd notify request/response %s/%s", request.c_str(), response.c_str());
        exit(EXIT_FAILURE);
    }

    if (g_notifySyncd == false)
    {
        SWSS_LOG_NOTICE("skipping notify syncd, selected by user");
        return;
    }

    // tell syncd that we are compiling new view
    sai_attribute_t attr;
    attr.id = SAI_REDIS_SWITCH_ATTR_NOTIFY_SYNCD;

    const std::string requestAction = r[2];

    if (requestAction == SYNCD_INIT_VIEW)
    {
        attr.value.s32 = SAI_REDIS_NOTIFY_SYNCD_INIT_VIEW;
    }
    else if (requestAction == SYNCD_APPLY_VIEW)
    {
        attr.value.s32 = SAI_REDIS_NOTIFY_SYNCD_APPLY_VIEW;
    }
    else
    {
        SWSS_LOG_ERROR("invalid syncd notify request: %s", request.c_str());
        exit(EXIT_FAILURE);
    }

    sai_status_t status = sai_switch_api->set_switch_attribute(&attr);

    const std::string& responseStatus = R[2];

    sai_status_t response_status;
    sai_deserialize_status(responseStatus, response_status);

    if (status != response_status)
    {
        SWSS_LOG_ERROR("response status %s is differnt than syncd status %s", responseStatus.c_str(), sai_serialize_status(status).c_str());
        exit(EXIT_FAILURE);
    }

    if (status != SAI_STATUS_SUCCESS)
    {
        SWSS_LOG_ERROR("failed to notify syncd %s", sai_serialize_status(status).c_str());
        exit(EXIT_FAILURE);
    }

    // OK
}

int replay(int argc, char **argv)
{
    //swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_DEBUG);

    SWSS_LOG_ENTER();

    if (argc < 2)
    {
        fprintf(stderr, "ERR: need to specify filename\n");

        return -1;
    }

    char* filename = argv[argc - 1];

    SWSS_LOG_NOTICE("using file: %s", filename);

    std::ifstream infile(filename);

    if (!infile.is_open())
    {
        SWSS_LOG_ERROR("failed to open file %s", filename);
        return -1;
    }

    std::string line;

    while (std::getline(infile, line))
    {
        // std::cout << "processing " << line << std::endl;

        sai_common_api_t api = SAI_COMMON_API_CREATE;

        auto p = line.find_first_of("|");

        char op = line[p+1];

        switch (op)
        {
            case 'a':
                {
                    std::string response;

                    do
                    {
                        // this line may be notification, we need to skip
                        if (!std::getline(infile, response))
                        {
                            SWSS_LOG_ERROR("failed to read next file from file, previous: %s", line.c_str());
                            exit(EXIT_FAILURE);
                        }
                    }
                    while (response[response.find_first_of("|")+1] == 'n');

                    performNotifySyncd(line, response);
                }
                continue;
            case 'S':
                performSleep(line);
                continue;
            case 'c':
                api = SAI_COMMON_API_CREATE;
                break;
            case 'r':
                api = SAI_COMMON_API_REMOVE;
                break;
            case 's':
                api = SAI_COMMON_API_SET;
                break;
            case 'g':
                api = SAI_COMMON_API_GET;
                break;
            case '#':
            case 'n':
                continue; // skip comment and notification

            default:
                SWSS_LOG_ERROR("unknown op %c", op);
                exit(EXIT_FAILURE);
        }

        // timestamp|action|objecttype:objectid|attrid=value,...
        auto fields = swss::tokenize(line, '|');

        // objecttype:objectid (object id may contain ':')
        auto start = fields[2].find_first_of(":");

        auto str_object_type = fields[2].substr(0, start);
        auto str_object_id  = fields[2].substr(start + 1);

        sai_object_type_t object_type = deserialize_object_type(str_object_type);

        auto values = get_values(fields);

        SaiAttributeList list(object_type, values, false);

        sai_attribute_t *attr_list = list.get_attr_list();

        uint32_t attr_count = list.get_attr_count();

        SWSS_LOG_DEBUG("attr count: %u", list.get_attr_count());

        if (api != SAI_COMMON_API_GET)
        {
            translate_local_to_redis(object_type, attr_count, attr_list);
        }

        sai_status_t status;

        switch (object_type)
        {
            case SAI_OBJECT_TYPE_FDB:
                status = handle_fdb(str_object_id, api, attr_count, attr_list);
                break;

            case SAI_OBJECT_TYPE_SWITCH:
                status = handle_switch(str_object_id, api, attr_count, attr_list);
                break;

            case SAI_OBJECT_TYPE_NEIGHBOR:
                status = handle_neighbor(str_object_id, api, attr_count, attr_list);
                break;

            case SAI_OBJECT_TYPE_ROUTE:
                status = handle_route(str_object_id, api, attr_count, attr_list);
                break;

            case SAI_OBJECT_TYPE_VLAN:
                status = handle_vlan(str_object_id, api, attr_count, attr_list);
                break;

            case SAI_OBJECT_TYPE_TRAP:
                status = handle_trap(str_object_id, api, attr_count, attr_list);
                break;

            default:
                status = handle_generic(object_type, str_object_id, api, attr_count, attr_list);
                break;
        }

        if (status != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("failed to execute api: %c: %d", op, status);

            exit(EXIT_FAILURE);
        }

        if (api == SAI_COMMON_API_GET)
        {
            std::string response;

            do
            {
                // this line may be notification, we need to skip
                std::getline(infile, response);
            }
            while (response[response.find_first_of("|")+1] == 'n');

            handle_get_response(object_type, attr_count, attr_list, response);
        }
    }

    infile.close();

    return 0;
}

void printUsage()
{
    std::cout << "Usage: saiplayer [-h] recordfile" << std::endl << std::endl;
    std::cout << "    -C --skipNotifySyncd:" << std::endl;
    std::cout << "        Will not send notify init/apply view to syncd" << std::endl << std::endl;
    std::cout << "    -d --enableDebug:" << std::endl;
    std::cout << "        Enable syslog debug messages" << std::endl << std::endl;
    std::cout << "    -u --useTempView:" << std::endl;
    std::cout << "        Enable temporary view between init and apply" << std::endl << std::endl;
    std::cout << "    -h --help:" << std::endl;
    std::cout << "        Print out this message" << std::endl << std::endl;
}

bool g_useTempView = false;

void handleCmdLine(int argc, char **argv)
{
    SWSS_LOG_ENTER();

    while(true)
    {
        static struct option long_options[] =
        {
            { "useTempView",      no_argument,       0, 'u' },
            { "help",             no_argument,       0, 'h' },
            { "skipNotifySyncd",  no_argument,       0, 'C' },
            { "enableDebug",      no_argument,       0, 'd' },
            { 0,                  0,                 0,  0  }
        };

        const char* const optstring = "hCdu";

        int option_index;

        int c = getopt_long(argc, argv, optstring, long_options, &option_index);

        if (c == -1)
            break;

        switch (c)
        {
            case 'd':
                swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_DEBUG);
                break;

            case 'u':
                g_useTempView = false;
                break;

            case 'C':
                g_notifySyncd = false;
                break;

            case 'h':
                printUsage();
                exit(EXIT_SUCCESS);

            case '?':
                SWSS_LOG_WARN("unknown option %c", optopt);
                printUsage();
                exit(EXIT_FAILURE);

            default:
                SWSS_LOG_ERROR("getopt_long failure");
                exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char **argv)
{
    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_DEBUG);

    SWSS_LOG_ENTER();

    swss::Logger::getInstance().setMinPrio(swss::Logger::SWSS_NOTICE);

    handleCmdLine(argc, argv);

    EXIT_ON_ERROR(sai_api_initialize(0, (const service_method_table_t *)&test_services));

    init_sai_api();

    initialize_common_api_pointers();

    EXIT_ON_ERROR(sai_switch_api->initialize_switch(0, "", "", &switch_notifications));

    sai_attribute_t attr;

    attr.id = SAI_REDIS_SWITCH_ATTR_USE_TEMP_VIEW;
    attr.value.booldata = g_useTempView;

    EXIT_ON_ERROR(sai_switch_api->set_switch_attribute(&attr));

    int exitcode = replay(argc, argv);

    sai_switch_api->shutdown_switch(false);

    sai_api_uninitialize();

    return exitcode;
}
