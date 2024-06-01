#pragma once

extern "C" {
#include "sai.h"
#include "saiextensions.h"
}

#include "meta/SaiInterface.h"

#include "swss/logger.h"

#include <memory>

#define PRIVATE __attribute__((visibility("hidden")))

PRIVATE extern const sai_acl_api_t                     proxy_acl_api;
PRIVATE extern const sai_ars_api_t                     proxy_ars_api;
PRIVATE extern const sai_ars_profile_api_t             proxy_ars_profile_api;
PRIVATE extern const sai_bfd_api_t                     proxy_bfd_api;
PRIVATE extern const sai_bmtor_api_t                   proxy_bmtor_api;
PRIVATE extern const sai_generic_programmable_api_t    proxy_generic_programmable_api;
PRIVATE extern const sai_bridge_api_t                  proxy_bridge_api;
PRIVATE extern const sai_buffer_api_t                  proxy_buffer_api;
PRIVATE extern const sai_counter_api_t                 proxy_counter_api;
PRIVATE extern const sai_dash_vip_api_t                proxy_dash_vip_api;
PRIVATE extern const sai_dash_pa_validation_api_t      proxy_dash_pa_validation_api;
PRIVATE extern const sai_dash_vnet_api_t               proxy_dash_vnet_api;
PRIVATE extern const sai_dash_outbound_routing_api_t   proxy_dash_outbound_routing_api;
PRIVATE extern const sai_dash_outbound_ca_to_pa_api_t  proxy_dash_outbound_ca_to_pa_api;
PRIVATE extern const sai_dash_inbound_routing_api_t    proxy_dash_inbound_routing_api;
PRIVATE extern const sai_dash_eni_api_t                proxy_dash_eni_api;
PRIVATE extern const sai_dash_direction_lookup_api_t   proxy_dash_direction_lookup_api;
PRIVATE extern const sai_dash_acl_api_t                proxy_dash_acl_api;
PRIVATE extern const sai_debug_counter_api_t           proxy_debug_counter_api;
PRIVATE extern const sai_dtel_api_t                    proxy_dtel_api;
PRIVATE extern const sai_fdb_api_t                     proxy_fdb_api;
PRIVATE extern const sai_hash_api_t                    proxy_hash_api;
PRIVATE extern const sai_hostif_api_t                  proxy_hostif_api;
PRIVATE extern const sai_ipmc_api_t                    proxy_ipmc_api;
PRIVATE extern const sai_ipmc_group_api_t              proxy_ipmc_group_api;
PRIVATE extern const sai_isolation_group_api_t         proxy_isolation_group_api;
PRIVATE extern const sai_l2mc_api_t                    proxy_l2mc_api;
PRIVATE extern const sai_l2mc_group_api_t              proxy_l2mc_group_api;
PRIVATE extern const sai_lag_api_t                     proxy_lag_api;
PRIVATE extern const sai_macsec_api_t                  proxy_macsec_api;
PRIVATE extern const sai_mcast_fdb_api_t               proxy_mcast_fdb_api;
PRIVATE extern const sai_mirror_api_t                  proxy_mirror_api;
PRIVATE extern const sai_mpls_api_t                    proxy_mpls_api;
PRIVATE extern const sai_nat_api_t                     proxy_nat_api;
PRIVATE extern const sai_neighbor_api_t                proxy_neighbor_api;
PRIVATE extern const sai_next_hop_api_t                proxy_next_hop_api;
PRIVATE extern const sai_next_hop_group_api_t          proxy_next_hop_group_api;
PRIVATE extern const sai_policer_api_t                 proxy_policer_api;
PRIVATE extern const sai_port_api_t                    proxy_port_api;
PRIVATE extern const sai_qos_map_api_t                 proxy_qos_map_api;
PRIVATE extern const sai_queue_api_t                   proxy_queue_api;
PRIVATE extern const sai_route_api_t                   proxy_route_api;
PRIVATE extern const sai_router_interface_api_t        proxy_router_interface_api;
PRIVATE extern const sai_rpf_group_api_t               proxy_rpf_group_api;
PRIVATE extern const sai_samplepacket_api_t            proxy_samplepacket_api;
PRIVATE extern const sai_scheduler_api_t               proxy_scheduler_api;
PRIVATE extern const sai_scheduler_group_api_t         proxy_scheduler_group_api;
PRIVATE extern const sai_srv6_api_t                    proxy_srv6_api;
PRIVATE extern const sai_stp_api_t                     proxy_stp_api;
PRIVATE extern const sai_switch_api_t                  proxy_switch_api;
PRIVATE extern const sai_system_port_api_t             proxy_system_port_api;
PRIVATE extern const sai_tam_api_t                     proxy_tam_api;
PRIVATE extern const sai_tunnel_api_t                  proxy_tunnel_api;
PRIVATE extern const sai_udf_api_t                     proxy_udf_api;
PRIVATE extern const sai_virtual_router_api_t          proxy_virtual_router_api;
PRIVATE extern const sai_vlan_api_t                    proxy_vlan_api;
PRIVATE extern const sai_wred_api_t                    proxy_wred_api;
PRIVATE extern const sai_my_mac_api_t                  proxy_my_mac_api;
PRIVATE extern const sai_ipsec_api_t                   proxy_ipsec_api;
PRIVATE extern const sai_twamp_api_t                   proxy_twamp_api;
PRIVATE extern const sai_dash_meter_api_t              proxy_dash_meter_api;
PRIVATE extern const sai_poe_api_t                     proxy_poe_api;

PRIVATE extern std::shared_ptr<sairedis::SaiInterface>   proxy_sai;

// QUAD OID

#define PROXY_CREATE(OT,ot)                             \
    static sai_status_t proxy_create_ ## ot(            \
            _Out_ sai_object_id_t *object_id,           \
            _In_ sai_object_id_t switch_id,             \
            _In_ uint32_t attr_count,                   \
            _In_ const sai_attribute_t *attr_list)      \
{                                                       \
    SWSS_LOG_ENTER();                                   \
    return proxy_sai->create(                           \
            (sai_object_type_t)SAI_OBJECT_TYPE_ ## OT,  \
            object_id,                                  \
            switch_id,                                  \
            attr_count,                                 \
            attr_list);                                 \
}

#define PROXY_REMOVE(OT,ot)                             \
    static sai_status_t proxy_remove_ ## ot(            \
            _In_ sai_object_id_t object_id)             \
{                                                       \
    SWSS_LOG_ENTER();                                   \
    return proxy_sai->remove(                           \
            (sai_object_type_t)SAI_OBJECT_TYPE_ ## OT,  \
            object_id);                                 \
}

#define PROXY_SET(OT,ot)                                \
    static sai_status_t proxy_set_ ## ot ## _attribute( \
            _In_ sai_object_id_t object_id,             \
            _In_ const sai_attribute_t *attr)           \
{                                                       \
    SWSS_LOG_ENTER();                                   \
    return proxy_sai->set(                              \
            (sai_object_type_t)SAI_OBJECT_TYPE_ ## OT,  \
            object_id,                                  \
            attr);                                      \
}

#define PROXY_GET(OT,ot)                                \
    static sai_status_t proxy_get_ ## ot ## _attribute( \
            _In_ sai_object_id_t object_id,             \
            _In_ uint32_t attr_count,                   \
            _Inout_ sai_attribute_t *attr_list)         \
{                                                       \
    SWSS_LOG_ENTER();                                   \
    return proxy_sai->get(                              \
            (sai_object_type_t)SAI_OBJECT_TYPE_ ## OT,  \
            object_id,                                  \
            attr_count,                                 \
            attr_list);                                 \
}

// QUAD DECLARE

#define PROXY_GENERIC_QUAD(OT,ot)  \
    PROXY_CREATE(OT,ot);           \
    PROXY_REMOVE(OT,ot);           \
    PROXY_SET(OT,ot);              \
    PROXY_GET(OT,ot);

// QUAD ENTRY

#define PROXY_CREATE_ENTRY(OT,ot)                       \
    static sai_status_t proxy_create_ ## ot(            \
            _In_ const sai_ ## ot ##_t *entry,          \
            _In_ uint32_t attr_count,                   \
            _In_ const sai_attribute_t *attr_list)      \
{                                                       \
    SWSS_LOG_ENTER();                                   \
    return proxy_sai->create(                           \
            entry,                                      \
            attr_count,                                 \
            attr_list);                                 \
}

#define PROXY_REMOVE_ENTRY(OT,ot)                       \
    static sai_status_t proxy_remove_ ## ot(            \
            _In_ const sai_ ## ot ## _t *entry)         \
{                                                       \
    SWSS_LOG_ENTER();                                   \
    return proxy_sai->remove(                           \
            entry);                                     \
}

#define PROXY_SET_ENTRY(OT,ot)                          \
    static sai_status_t proxy_set_ ## ot ## _attribute( \
            _In_ const sai_ ## ot ## _t *entry,         \
            _In_ const sai_attribute_t *attr)           \
{                                                       \
    SWSS_LOG_ENTER();                                   \
    return proxy_sai->set(                              \
            entry,                                      \
            attr);                                      \
}

#define PROXY_GET_ENTRY(OT,ot)                          \
    static sai_status_t proxy_get_ ## ot ## _attribute( \
            _In_ const sai_ ## ot ## _t *entry,         \
            _In_ uint32_t attr_count,                   \
            _Inout_ sai_attribute_t *attr_list)         \
{                                                       \
    SWSS_LOG_ENTER();                                   \
    return proxy_sai->get(                              \
            entry,                                      \
            attr_count,                                 \
            attr_list);                                 \
}

// QUAD ENTRY DECLARE

#define PROXY_GENERIC_QUAD_ENTRY(OT,ot)    \
    PROXY_CREATE_ENTRY(OT,ot);             \
    PROXY_REMOVE_ENTRY(OT,ot);             \
    PROXY_SET_ENTRY(OT,ot);                \
    PROXY_GET_ENTRY(OT,ot);

// QUAD API

#define PROXY_GENERIC_QUAD_API(ot)      \
    proxy_create_ ## ot,                \
    proxy_remove_ ## ot,                \
    proxy_set_ ## ot ##_attribute,      \
    proxy_get_ ## ot ##_attribute,

// STATS

#define PROXY_GET_STATS(OT,ot)                          \
    static sai_status_t proxy_get_ ## ot ## _stats(     \
            _In_ sai_object_id_t object_id,             \
            _In_ uint32_t number_of_counters,           \
            _In_ const sai_stat_id_t *counter_ids,      \
            _Out_ uint64_t *counters)                   \
{                                                       \
    SWSS_LOG_ENTER();                                   \
    return proxy_sai->getStats(                         \
            (sai_object_type_t)SAI_OBJECT_TYPE_ ## OT,  \
            object_id,                                  \
            number_of_counters,                         \
            counter_ids,                                \
            counters);                                  \
}

#define PROXY_GET_STATS_EXT(OT,ot)                      \
    static sai_status_t proxy_get_ ## ot ## _stats_ext( \
            _In_ sai_object_id_t object_id,             \
            _In_ uint32_t number_of_counters,           \
            _In_ const sai_stat_id_t *counter_ids,      \
            _In_ sai_stats_mode_t mode,                 \
            _Out_ uint64_t *counters)                   \
{                                                       \
    SWSS_LOG_ENTER();                                   \
    return proxy_sai->getStatsExt(                      \
            (sai_object_type_t)SAI_OBJECT_TYPE_ ## OT,  \
            object_id,                                  \
            number_of_counters,                         \
            counter_ids,                                \
            mode,                                       \
            counters);                                  \
}

#define PROXY_CLEAR_STATS(OT,ot)                        \
    static sai_status_t proxy_clear_ ## ot ## _stats(   \
            _In_ sai_object_id_t object_id,             \
            _In_ uint32_t number_of_counters,           \
            _In_ const sai_stat_id_t *counter_ids)      \
{                                                       \
    SWSS_LOG_ENTER();                                   \
    return proxy_sai->clearStats(                       \
            (sai_object_type_t)SAI_OBJECT_TYPE_ ## OT,  \
            object_id,                                  \
            number_of_counters,                         \
            counter_ids);                               \
}

// STATS DECLARE

#define PROXY_GENERIC_STATS(OT, ot)    \
    PROXY_GET_STATS(OT,ot);            \
    PROXY_GET_STATS_EXT(OT,ot);        \
    PROXY_CLEAR_STATS(OT,ot);

// STATS API

#define PROXY_GENERIC_STATS_API(ot)     \
    proxy_get_ ## ot ## _stats,         \
    proxy_get_ ## ot ## _stats_ext,     \
    proxy_clear_ ## ot ## _stats,

// BULK QUAD

#define PROXY_BULK_CREATE(OT,fname)                    \
    static sai_status_t proxy_bulk_create_ ## fname(   \
            _In_ sai_object_id_t switch_id,            \
            _In_ uint32_t object_count,                \
            _In_ const uint32_t *attr_count,           \
            _In_ const sai_attribute_t **attr_list,    \
            _In_ sai_bulk_op_error_mode_t mode,        \
            _Out_ sai_object_id_t *object_id,          \
            _Out_ sai_status_t *object_statuses)       \
{                                                      \
    SWSS_LOG_ENTER();                                  \
    return proxy_sai->bulkCreate(                      \
            (sai_object_type_t)SAI_OBJECT_TYPE_ ## OT, \
            switch_id,                                 \
            object_count,                              \
            attr_count,                                \
            attr_list,                                 \
            mode,                                      \
            object_id,                                 \
            object_statuses);                          \
}

#define PROXY_BULK_REMOVE(OT,fname)                    \
    static sai_status_t proxy_bulk_remove_ ## fname(   \
            _In_ uint32_t object_count,                \
            _In_ const sai_object_id_t *object_id,     \
            _In_ sai_bulk_op_error_mode_t mode,        \
            _Out_ sai_status_t *object_statuses)       \
{                                                      \
    SWSS_LOG_ENTER();                                  \
    return proxy_sai->bulkRemove(                      \
            (sai_object_type_t)SAI_OBJECT_TYPE_ ## OT, \
            object_count,                              \
            object_id,                                 \
            mode,                                      \
            object_statuses);                          \
}

#define PROXY_BULK_SET(OT,fname)                       \
    static sai_status_t proxy_bulk_set_ ## fname(      \
            _In_ uint32_t object_count,                \
            _In_ const sai_object_id_t *object_id,     \
            _In_ const sai_attribute_t *attr_list,     \
            _In_ sai_bulk_op_error_mode_t mode,        \
            _Out_ sai_status_t *object_statuses)       \
{                                                      \
    SWSS_LOG_ENTER();                                  \
    return proxy_sai->bulkSet(                         \
            (sai_object_type_t)SAI_OBJECT_TYPE_ ## OT, \
            object_count,                              \
            object_id,                                 \
            attr_list,                                 \
            mode,                                      \
            object_statuses);                          \
}

#define PROXY_BULK_GET(OT,fname)                    \
    static sai_status_t proxy_bulk_get_ ## fname(   \
            _In_ uint32_t object_count,             \
            _In_ const sai_object_id_t *object_id,  \
            _In_ const uint32_t *attr_count,        \
            _Inout_ sai_attribute_t **attr_list,    \
            _In_ sai_bulk_op_error_mode_t mode,     \
            _Out_ sai_status_t *object_statuses)    \
{                                                   \
    SWSS_LOG_ENTER();                               \
    SWSS_LOG_ERROR("not implemented");              \
    return SAI_STATUS_NOT_IMPLEMENTED;              \
}

// BULK QUAD DECLARE

#define PROXY_BULK_QUAD(OT,ot)     \
    PROXY_BULK_CREATE(OT,ot);      \
    PROXY_BULK_REMOVE(OT,ot);      \
    PROXY_BULK_SET(OT,ot);         \
    PROXY_BULK_GET(OT,ot);

// BULK QUAD ENTRY

#define PROXY_BULK_CREATE_ENTRY(OT,ot)              \
    PROXY_BULK_CREATE_ENTRY_EX(OT, ot, ot)

#define PROXY_BULK_CREATE_ENTRY_EX(OT,ot,fname)     \
    static sai_status_t proxy_bulk_create_ ## fname(\
            _In_ uint32_t object_count,             \
            _In_ const sai_ ## ot ## _t *entry,     \
            _In_ const uint32_t *attr_count,        \
            _In_ const sai_attribute_t **attr_list, \
            _In_ sai_bulk_op_error_mode_t mode,     \
            _Out_ sai_status_t *object_statuses)    \
{                                                   \
    SWSS_LOG_ENTER();                               \
    return proxy_sai->bulkCreate(                   \
            object_count,                           \
            entry,                                  \
            attr_count,                             \
            attr_list,                              \
            mode,                                   \
            object_statuses);                       \
}

#define PROXY_BULK_REMOVE_ENTRY(OT,ot)              \
    PROXY_BULK_REMOVE_ENTRY_EX(OT, ot, ot)

#define PROXY_BULK_REMOVE_ENTRY_EX(OT,ot,fname)     \
    static sai_status_t proxy_bulk_remove_ ## fname(\
            _In_ uint32_t object_count,             \
            _In_ const sai_ ## ot ##_t *entry,      \
            _In_ sai_bulk_op_error_mode_t mode,     \
            _Out_ sai_status_t *object_statuses)    \
{                                                   \
    SWSS_LOG_ENTER();                               \
    return proxy_sai->bulkRemove(                   \
            object_count,                           \
            entry,                                  \
            mode,                                   \
            object_statuses);                       \
}

#define PROXY_BULK_SET_ENTRY(OT,ot)                 \
    static sai_status_t proxy_bulk_set_ ## ot(      \
            _In_ uint32_t object_count,             \
            _In_ const sai_ ## ot ## _t *entry,     \
            _In_ const sai_attribute_t *attr_list,  \
            _In_ sai_bulk_op_error_mode_t mode,     \
            _Out_ sai_status_t *object_statuses)    \
{                                                   \
    SWSS_LOG_ENTER();                               \
    return proxy_sai->bulkSet(                      \
            object_count,                           \
            entry,                                  \
            attr_list,                              \
            mode,                                   \
            object_statuses);                       \
}

#define PROXY_BULK_GET_ENTRY(OT,ot)                 \
    static sai_status_t proxy_bulk_get_ ## ot(      \
            _In_ uint32_t object_count,             \
            _In_ const sai_ ## ot ## _t *entry,     \
            _In_ const uint32_t *attr_count,        \
            _Inout_ sai_attribute_t **attr_list,    \
            _In_ sai_bulk_op_error_mode_t mode,     \
            _Out_ sai_status_t *object_statuses)    \
{                                                   \
    SWSS_LOG_ENTER();                               \
    SWSS_LOG_ERROR("not implemented");              \
    return SAI_STATUS_NOT_IMPLEMENTED;              \
}

// BULK QUAD ENTRY DECLARE

#define PROXY_BULK_QUAD_ENTRY(OT,ot)   \
    PROXY_BULK_CREATE_ENTRY(OT,ot);    \
    PROXY_BULK_REMOVE_ENTRY(OT,ot);    \
    PROXY_BULK_SET_ENTRY(OT,ot);       \
    PROXY_BULK_GET_ENTRY(OT,ot);

// BULK QUAD API

#define PROXY_BULK_QUAD_API(ot)     \
    proxy_bulk_create_ ## ot,       \
    proxy_bulk_remove_ ## ot,       \
    proxy_bulk_set_ ## ot,          \
    proxy_bulk_get_ ## ot,

// BULK get/set DECLARE

#define PROXY_BULK_GET_SET(OT,ot)   \
    PROXY_BULK_GET(OT,ot);          \
    PROXY_BULK_SET(OT,ot);

// BULK get/set API

#define PROXY_BULK_GET_SET_API(ot)     \
    proxy_bulk_get_ ## ot,             \
    proxy_bulk_set_ ## ot,
