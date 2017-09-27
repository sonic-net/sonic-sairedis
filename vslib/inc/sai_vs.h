#ifndef __SAI_VS__
#define __SAI_VS__

extern "C" {
#include "sai.h"
}

#include "swss/logger.h"
#include "meta/sai_meta.h"

#include <mutex>

#define SAI_KEY_VS_SWITCH_TYPE "SAI_VS_SWITCH_TYPE"

// TODO probaby should be per switch
#define SAI_VALUE_VS_SWITCH_TYPE_BCM56850     "SAI_VS_SWITCH_TYPE_BCM56850"
#define SAI_VALUE_VS_SWITCH_TYPE_MLNX2700     "SAI_VS_SWITCH_TYPE_MLNX2700"

typedef enum _sai_vs_switch_type_t
{
    SAI_VS_SWITCH_TYPE_NONE,

    SAI_VS_SWITCH_TYPE_BCM56850,

    SAI_VS_SWITCH_TYPE_MLNX2700,

} sai_vs_switch_type_t;

extern sai_vs_switch_type_t             g_vs_switch_type;
extern std::recursive_mutex             g_recursive_mutex;

extern const sai_acl_api_t              vs_acl_api;
extern const sai_bridge_api_t           vs_bridge_api;
extern const sai_buffer_api_t           vs_buffer_api;
extern const sai_fdb_api_t              vs_fdb_api;
extern const sai_hash_api_t             vs_hash_api;
extern const sai_hostif_api_t           vs_hostif_api;
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
extern const sai_virtual_router_api_t   vs_virtual_router_api;
extern const sai_vlan_api_t             vs_vlan_api;
extern const sai_wred_api_t             vs_wred_api;

// CREATE

sai_status_t vs_generic_create(
        _In_ sai_object_type_t object_type,
        _Out_ sai_object_id_t *object_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list);

sai_status_t vs_generic_create_fdb_entry(
        _In_ const sai_fdb_entry_t *fdb_entry,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list);

sai_status_t vs_generic_create_neighbor_entry(
        _In_ const sai_neighbor_entry_t *neighbor_entry,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list);

sai_status_t vs_generic_create_route_entry(
        _In_ const sai_route_entry_t *route_entry,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list);

// REMOVE

sai_status_t vs_generic_remove(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id);

sai_status_t vs_generic_remove_fdb_entry(
        _In_ const sai_fdb_entry_t *fdb_entry);

sai_status_t vs_generic_remove_neighbor_entry(
        _In_ const sai_neighbor_entry_t *neighbor_entry);

sai_status_t vs_generic_remove_route_entry(
        _In_ const sai_route_entry_t *route_entry);

// SET

sai_status_t vs_generic_set(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ const sai_attribute_t *attr);

sai_status_t vs_generic_set_fdb_entry(
        _In_ const sai_fdb_entry_t *fdb_entry,
        _In_ const sai_attribute_t *attr);

sai_status_t vs_generic_set_neighbor_entry(
        _In_ const sai_neighbor_entry_t *neighbor_entry,
        _In_ const sai_attribute_t *attr);

sai_status_t vs_generic_set_route_entry(
        _In_ const sai_route_entry_t *route_entry,
        _In_ const sai_attribute_t *attr);

// GET

sai_status_t vs_generic_get(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list);

sai_status_t vs_generic_get_fdb_entry(
        _In_ const sai_fdb_entry_t *fdb_entry,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list);

sai_status_t vs_generic_get_neighbor_entry(
        _In_ const sai_neighbor_entry_t *neighbor_entry,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list);

sai_status_t vs_generic_get_route_entry(
        _In_ const sai_route_entry_t *route_entry,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list);

#endif // __SAI_VS__
