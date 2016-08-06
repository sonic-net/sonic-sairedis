#ifndef __SAI_VS__
#define __SAI_VS__

#include <mutex>
#include <unordered_map>

#include "stdint.h"
#include "stdio.h"

extern "C" {
#include "sai.h"
}
#include "saiserialize.h"
#include "saiattributelist.h"

#include "swss/logger.h"

extern service_method_table_t           g_services;

extern uint64_t g_real_id;

typedef std::unordered_map<std::string, std::string> AttrHash;
typedef std::unordered_map<std::string, AttrHash> ObjectHash;

// first key is serialized object ID (object id, vlan, route, neighbor, fdb)
// value is hash containing attributes
// second key is serialized attribute ID for given object
// value is serialized attribute value
extern ObjectHash g_objectHash;

extern std::recursive_mutex g_recursive_mutex;

extern const sai_acl_api_t              vs_acl_api;
extern const sai_buffer_api_t           vs_buffer_api;
extern const sai_fdb_api_t              vs_fdb_api;
extern const sai_hash_api_t             vs_hash_api;
extern const sai_hostif_api_t           vs_host_interface_api;
extern const sai_lag_api_t              vs_lag_api;
extern const sai_mirror_api_t           vs_mirror_api;
extern const sai_neighbor_api_t         vs_neighbor_api;
extern const sai_next_hop_api_t         vs_next_hop_api;
extern const sai_next_hop_group_api_t   vs_next_hop_group_api;
extern const sai_policer_api_t          vs_policer_api;
extern const sai_port_api_t             vs_port_api;
extern const sai_qos_map_api_t          vs_qos_map_api;
extern const sai_queue_api_t            vs_queue_api;
extern const sai_route_api_t            vs_route_api;
extern const sai_router_interface_api_t vs_router_interface_api;
extern const sai_samplepacket_api_t     vs_samplepacket_api;
extern const sai_scheduler_api_t        vs_scheduler_api;
extern const sai_scheduler_group_api_t  vs_scheduler_group_api;
extern const sai_stp_api_t              vs_stp_api;
extern const sai_switch_api_t           vs_switch_api;
extern const sai_tunnel_api_t           vs_tunnel_api;
extern const sai_udf_api_t              vs_udf_api;
extern const sai_virtual_router_api_t   vs_router_api;
extern const sai_vlan_api_t             vs_vlan_api;
extern const sai_wred_api_t             vs_wred_api;

extern sai_switch_notification_t vs_switch_notifications;

#define UNREFERENCED_PARAMETER(X)

void translate_rid_to_vid(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t attr_count,
        _In_ sai_attribute_t *attr_list);

// separate methods are needed for vlan to not confuse with object_id

sai_status_t vs_generic_create(
        _In_ sai_object_type_t object_type,
        _Out_ sai_object_id_t* object_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list);

sai_status_t vs_generic_create(
        _In_ sai_object_type_t object_type,
        _In_ const sai_fdb_entry_t *fdb_entry,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list);

sai_status_t vs_generic_create(
        _In_ sai_object_type_t object_type,
        _In_ const sai_neighbor_entry_t* neighbor_entry,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list);

sai_status_t vs_generic_create(
        _In_ sai_object_type_t object_type,
        _In_ const sai_unicast_route_entry_t* unicast_route_entry,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list);

sai_status_t vs_generic_create_vlan(
        _In_ sai_object_type_t object_type,
        _In_ sai_vlan_id_t vlan_id);


sai_status_t vs_generic_remove(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id);

sai_status_t vs_generic_remove(
        _In_ sai_object_type_t object_type,
        _In_ const sai_fdb_entry_t* fdb_entry);

sai_status_t vs_generic_remove(
        _In_ sai_object_type_t object_type,
        _In_ const sai_neighbor_entry_t* neighbor_entry);

sai_status_t vs_generic_remove(
        _In_ sai_object_type_t object_type,
        _In_ const sai_unicast_route_entry_t* unicast_route_entry);

sai_status_t vs_generic_remove_vlan(
        _In_ sai_object_type_t object_type,
        _In_ sai_vlan_id_t vlan_id);


sai_status_t vs_generic_set(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ const sai_attribute_t *attr);

sai_status_t vs_generic_set(
        _In_ sai_object_type_t object_type,
        _In_ const sai_fdb_entry_t *fdb_entry,
        _In_ const sai_attribute_t *attr);

sai_status_t vs_generic_set(
        _In_ sai_object_type_t object_type,
        _In_ const sai_neighbor_entry_t* neighbor_entry,
        _In_ const sai_attribute_t *attr);

sai_status_t vs_generic_set(
        _In_ sai_object_type_t object_type,
        _In_ const sai_unicast_route_entry_t* unicast_route_entry,
        _In_ const sai_attribute_t *attr);

sai_status_t vs_generic_set_vlan(
        _In_ sai_object_type_t object_type,
        _In_ sai_vlan_id_t vlan_id,
        _In_ const sai_attribute_t *attr);


sai_status_t vs_generic_get(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list);

sai_status_t vs_generic_get(
        _In_ sai_object_type_t object_type,
        _In_ const sai_fdb_entry_t *fdb_entry,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list);

sai_status_t vs_generic_get(
        _In_ sai_object_type_t object_type,
        _In_ const sai_neighbor_entry_t* neighbor_entry,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list);

sai_status_t vs_generic_get(
        _In_ sai_object_type_t object_type,
        _In_ const sai_unicast_route_entry_t* unicast_route_entry,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list);

sai_status_t vs_generic_get_vlan(
        _In_ sai_object_type_t object_type,
        _In_ sai_vlan_id_t vlan_id,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list);

// notifications

void handle_notification(
        _In_ const std::string &notification,
        _In_ const std::string &data,
        _In_ const std::vector<swss::FieldValueTuple> &values);

#endif // __SAI_VS__
