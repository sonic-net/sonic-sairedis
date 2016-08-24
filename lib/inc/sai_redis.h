#ifndef __SAI_REDIS__
#define __SAI_REDIS__

#include <mutex>
#include <set>

#include "stdint.h"
#include "stdio.h"

extern "C" {
#include "sai.h"
}
#include "saiserialize.h"
#include "saiattributelist.h"
#include "redisclient.h"

#include "swss/dbconnector.h"
#include "swss/producertable.h"
#include "swss/consumertable.h"
#include "swss/notificationconsumer.h"
#include "swss/notificationproducer.h"
#include "swss/table.h"
#include "swss/select.h"
#include "swss/logger.h"

#define DEFAULT_VLAN_NUMBER 1

// local redis state
extern std::set<sai_object_id_t>        local_next_hops_set;
extern std::set<sai_object_id_t>        local_router_interfaces_set;
extern std::set<sai_object_id_t>        local_next_hop_groups_set;
extern std::set<sai_object_id_t>        local_lags_set;
extern std::set<sai_object_id_t>        local_lag_members_set;
extern std::set<std::string>            local_neighbor_entries_set;
extern std::set<std::string>            local_route_entries_set;
extern std::set<sai_object_id_t>        local_virtual_routers_set;
extern sai_object_id_t                  local_default_virtual_router_id;
extern sai_object_id_t                  local_cpu_port_id;
extern std::set<sai_vlan_id_t>          local_vlans_set;
extern std::set<sai_object_id_t>        local_vlan_members_set;
extern std::set<sai_object_id_t>        local_tunnel_maps_set;
extern std::set<sai_object_id_t>        local_tunnels_set;
extern std::set<sai_object_id_t>        local_tunnel_term_table_entries_set;
extern std::set<sai_object_id_t>        local_ports_set;
extern std::set<sai_object_id_t>        local_policers_set;
extern std::set<sai_object_id_t>        local_switches_set;
extern std::set<sai_object_id_t>        local_hostif_trap_groups_set;
extern std::set<sai_object_id_t>        local_hostifs_set;

// other global declarations

extern service_method_table_t           g_services;
extern swss::DBConnector               *g_db;
extern swss::ProducerTable             *g_asicState;

extern swss::NotificationProducer      *g_notifySyncdProducer;
extern swss::ProducerTable             *g_redisGetProducer;
extern swss::ConsumerTable             *g_redisGetConsumer;
extern swss::NotificationConsumer      *g_redisNotifications;
extern swss::NotificationConsumer      *g_notifySyncdConsumer;

extern swss::Table *g_vidToRid;
extern swss::Table *g_ridToVid;

extern swss::RedisClient               *g_redisClient;

extern std::mutex g_mutex;
extern std::mutex g_apimutex;

extern const sai_acl_api_t              redis_acl_api;
extern const sai_buffer_api_t           redis_buffer_api;
extern const sai_fdb_api_t              redis_fdb_api;
extern const sai_hash_api_t             redis_hash_api;
extern const sai_hostif_api_t           redis_host_interface_api;
extern const sai_lag_api_t              redis_lag_api;
extern const sai_mirror_api_t           redis_mirror_api;
extern const sai_neighbor_api_t         redis_neighbor_api;
extern const sai_next_hop_api_t         redis_next_hop_api;
extern const sai_next_hop_group_api_t   redis_next_hop_group_api;
extern const sai_policer_api_t          redis_policer_api;
extern const sai_port_api_t             redis_port_api;
extern const sai_qos_map_api_t          redis_qos_map_api;
extern const sai_queue_api_t            redis_queue_api;
extern const sai_route_api_t            redis_route_api;
extern const sai_router_interface_api_t redis_router_interface_api;
extern const sai_samplepacket_api_t     redis_samplepacket_api;
extern const sai_scheduler_api_t        redis_scheduler_api;
extern const sai_scheduler_group_api_t  redis_scheduler_group_api;
extern const sai_stp_api_t              redis_stp_api;
extern const sai_switch_api_t           redis_switch_api;
extern const sai_tunnel_api_t           redis_tunnel_api;
extern const sai_udf_api_t              redis_udf_api;
extern const sai_virtual_router_api_t   redis_router_api;
extern const sai_vlan_api_t             redis_vlan_api;
extern const sai_wred_api_t             redis_wred_api;

extern sai_switch_notification_t redis_switch_notifications;

#define UNREFERENCED_PARAMETER(X)

bool redis_validate_contains_attribute(
    _In_ sai_attr_id_t required_id,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list);

const sai_attribute_t* redis_get_attribute_by_id(
    _In_ sai_attr_id_t id,
    _In_ uint32_t attr_count,
    _In_ const sai_attribute_t *attr_list);

sai_object_id_t redis_create_virtual_object_id(
        _In_ sai_object_type_t object_type);

void translate_rid_to_vid(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list);

// separate methods are needed for vlan to not confuse with object_id

sai_status_t redis_generic_create(
        _In_ sai_object_type_t object_type,
        _Out_ sai_object_id_t* object_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list);

sai_status_t redis_generic_create(
        _In_ sai_object_type_t object_type,
        _In_ const sai_fdb_entry_t *fdb_entry,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list);

sai_status_t redis_generic_create(
        _In_ sai_object_type_t object_type,
        _In_ const sai_neighbor_entry_t* neighbor_entry,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list);

sai_status_t redis_generic_create(
        _In_ sai_object_type_t object_type,
        _In_ const sai_unicast_route_entry_t* unicast_route_entry,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list);

sai_status_t redis_generic_create_vlan(
        _In_ sai_object_type_t object_type,
        _In_ sai_vlan_id_t vlan_id);


sai_status_t redis_generic_remove(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id);

sai_status_t redis_generic_remove(
        _In_ sai_object_type_t object_type,
        _In_ const sai_fdb_entry_t* fdb_entry);

sai_status_t redis_generic_remove(
        _In_ sai_object_type_t object_type,
        _In_ const sai_neighbor_entry_t* neighbor_entry);

sai_status_t redis_generic_remove(
        _In_ sai_object_type_t object_type,
        _In_ const sai_unicast_route_entry_t* unicast_route_entry);

sai_status_t redis_generic_remove_vlan(
        _In_ sai_object_type_t object_type,
        _In_ sai_vlan_id_t vlan_id);


sai_status_t redis_generic_set(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ const sai_attribute_t *attr);

sai_status_t redis_generic_set(
        _In_ sai_object_type_t object_type,
        _In_ const sai_fdb_entry_t *fdb_entry,
        _In_ const sai_attribute_t *attr);

sai_status_t redis_generic_set(
        _In_ sai_object_type_t object_type,
        _In_ const sai_neighbor_entry_t* neighbor_entry,
        _In_ const sai_attribute_t *attr);

sai_status_t redis_generic_set(
        _In_ sai_object_type_t object_type,
        _In_ const sai_unicast_route_entry_t* unicast_route_entry,
        _In_ const sai_attribute_t *attr);

sai_status_t redis_generic_set_vlan(
        _In_ sai_object_type_t object_type,
        _In_ sai_vlan_id_t vlan_id,
        _In_ const sai_attribute_t *attr);


sai_status_t redis_generic_get(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list);

sai_status_t redis_generic_get(
        _In_ sai_object_type_t object_type,
        _In_ const sai_fdb_entry_t *fdb_entry,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list);

sai_status_t redis_generic_get(
        _In_ sai_object_type_t object_type,
        _In_ const sai_neighbor_entry_t* neighbor_entry,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list);

sai_status_t redis_generic_get(
        _In_ sai_object_type_t object_type,
        _In_ const sai_unicast_route_entry_t* unicast_route_entry,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list);

sai_status_t redis_generic_get_vlan(
        _In_ sai_object_type_t object_type,
        _In_ sai_vlan_id_t vlan_id,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list);

// notifications

void handle_notification(
        _In_ const std::string &notification,
        _In_ const std::string &data,
        _In_ const std::vector<swss::FieldValueTuple> &values);

#endif // __SAI_REDIS__
