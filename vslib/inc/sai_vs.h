#pragma once

extern "C" {
#include "sai.h"
#include "saiextensions.h"
}

#include "Sai.h"

#include <memory>

#define PRIVATE __attribute__((visibility("hidden")))

PRIVATE extern const sai_acl_api_t              vs_acl_api;
PRIVATE extern const sai_bfd_api_t              vs_bfd_api;
PRIVATE extern const sai_bmtor_api_t            vs_bmtor_api;
PRIVATE extern const sai_bridge_api_t           vs_bridge_api;
PRIVATE extern const sai_buffer_api_t           vs_buffer_api;
PRIVATE extern const sai_counter_api_t          vs_counter_api;
PRIVATE extern const sai_debug_counter_api_t    vs_debug_counter_api;
PRIVATE extern const sai_dtel_api_t             vs_dtel_api;
PRIVATE extern const sai_fdb_api_t              vs_fdb_api;
PRIVATE extern const sai_hash_api_t             vs_hash_api;
PRIVATE extern const sai_hostif_api_t           vs_hostif_api;
PRIVATE extern const sai_ipmc_api_t             vs_ipmc_api;
PRIVATE extern const sai_ipmc_group_api_t       vs_ipmc_group_api;
PRIVATE extern const sai_isolation_group_api_t  vs_isolation_group_api;
PRIVATE extern const sai_l2mc_api_t             vs_l2mc_api;
PRIVATE extern const sai_l2mc_group_api_t       vs_l2mc_group_api;
PRIVATE extern const sai_lag_api_t              vs_lag_api;
PRIVATE extern const sai_macsec_api_t           vs_macsec_api;
PRIVATE extern const sai_mcast_fdb_api_t        vs_mcast_fdb_api;
PRIVATE extern const sai_mirror_api_t           vs_mirror_api;
PRIVATE extern const sai_mpls_api_t             vs_mpls_api;
PRIVATE extern const sai_nat_api_t              vs_nat_api;
PRIVATE extern const sai_neighbor_api_t         vs_neighbor_api;
PRIVATE extern const sai_next_hop_api_t         vs_next_hop_api;
PRIVATE extern const sai_next_hop_group_api_t   vs_next_hop_group_api;
PRIVATE extern const sai_policer_api_t          vs_policer_api;
PRIVATE extern const sai_port_api_t             vs_port_api;
PRIVATE extern const sai_qos_map_api_t          vs_qos_map_api;
PRIVATE extern const sai_queue_api_t            vs_queue_api;
PRIVATE extern const sai_route_api_t            vs_route_api;
PRIVATE extern const sai_router_interface_api_t vs_router_interface_api;
PRIVATE extern const sai_rpf_group_api_t        vs_rpf_group_api;
PRIVATE extern const sai_samplepacket_api_t     vs_samplepacket_api;
PRIVATE extern const sai_scheduler_api_t        vs_scheduler_api;
PRIVATE extern const sai_scheduler_group_api_t  vs_scheduler_group_api;
PRIVATE extern const sai_segmentroute_api_t     vs_segmentroute_api;
PRIVATE extern const sai_stp_api_t              vs_stp_api;
PRIVATE extern const sai_switch_api_t           vs_switch_api;
PRIVATE extern const sai_system_port_api_t      vs_system_port_api;
PRIVATE extern const sai_tam_api_t              vs_tam_api;
PRIVATE extern const sai_tunnel_api_t           vs_tunnel_api;
PRIVATE extern const sai_udf_api_t              vs_udf_api;
PRIVATE extern const sai_virtual_router_api_t   vs_virtual_router_api;
PRIVATE extern const sai_vlan_api_t             vs_vlan_api;
PRIVATE extern const sai_wred_api_t             vs_wred_api;

PRIVATE extern std::shared_ptr<saivs::Sai>      vs_sai;

// QUAD OID

#define VS_CREATE(OT,ot)                                \
    static sai_status_t vs_create_ ## ot(               \
            _Out_ sai_object_id_t *object_id,           \
            _In_ sai_object_id_t switch_id,             \
            _In_ uint32_t attr_count,                   \
            _In_ const sai_attribute_t *attr_list)      \
{                                                       \
    SWSS_LOG_ENTER();                                   \
    return vs_sai->create(                              \
            (sai_object_type_t)SAI_OBJECT_TYPE_ ## OT,  \
            object_id,                                  \
            switch_id,                                  \
            attr_count,                                 \
            attr_list);                                 \
}

#define VS_REMOVE(OT,ot)                                \
    static sai_status_t vs_remove_ ## ot(               \
            _In_ sai_object_id_t object_id)             \
{                                                       \
    SWSS_LOG_ENTER();                                   \
    return vs_sai->remove(                              \
            (sai_object_type_t)SAI_OBJECT_TYPE_ ## OT,  \
            object_id);                                 \
}

#define VS_SET(OT,ot)                                   \
    static sai_status_t vs_set_ ## ot ## _attribute(    \
            _In_ sai_object_id_t object_id,             \
            _In_ const sai_attribute_t *attr)           \
{                                                       \
    SWSS_LOG_ENTER();                                   \
    return vs_sai->set(                                 \
            (sai_object_type_t)SAI_OBJECT_TYPE_ ## OT,  \
            object_id,                                  \
            attr);                                      \
}

#define VS_GET(OT,ot)                                   \
    static sai_status_t vs_get_ ## ot ## _attribute(    \
            _In_ sai_object_id_t object_id,             \
            _In_ uint32_t attr_count,                   \
            _Inout_ sai_attribute_t *attr_list)         \
{                                                       \
    SWSS_LOG_ENTER();                                   \
    return vs_sai->get(                                 \
            (sai_object_type_t)SAI_OBJECT_TYPE_ ## OT,  \
            object_id,                                  \
            attr_count,                                 \
            attr_list);                                 \
}

// QUAD DECLARE

#define VS_GENERIC_QUAD(OT,ot)  \
    VS_CREATE(OT,ot);           \
    VS_REMOVE(OT,ot);           \
    VS_SET(OT,ot);              \
    VS_GET(OT,ot);

// QUAD ENTRY

#define VS_CREATE_ENTRY(OT,ot)                          \
    static sai_status_t vs_create_ ## ot(               \
            _In_ const sai_ ## ot ##_t *entry,          \
            _In_ uint32_t attr_count,                   \
            _In_ const sai_attribute_t *attr_list)      \
{                                                       \
    SWSS_LOG_ENTER();                                   \
    return vs_sai->create(                              \
            entry,                                      \
            attr_count,                                 \
            attr_list);                                 \
}

#define VS_REMOVE_ENTRY(OT,ot)                          \
    static sai_status_t vs_remove_ ## ot(               \
            _In_ const sai_ ## ot ## _t *entry)         \
{                                                       \
    SWSS_LOG_ENTER();                                   \
    return vs_sai->remove(                              \
            entry);                                     \
}

#define VS_SET_ENTRY(OT,ot)                             \
    static sai_status_t vs_set_ ## ot ## _attribute(    \
            _In_ const sai_ ## ot ## _t *entry,         \
            _In_ const sai_attribute_t *attr)           \
{                                                       \
    SWSS_LOG_ENTER();                                   \
    return vs_sai->set(                                 \
            entry,                                      \
            attr);                                      \
}

#define VS_GET_ENTRY(OT,ot)                             \
    static sai_status_t vs_get_ ## ot ## _attribute(    \
            _In_ const sai_ ## ot ## _t *entry,         \
            _In_ uint32_t attr_count,                   \
            _Inout_ sai_attribute_t *attr_list)         \
{                                                       \
    SWSS_LOG_ENTER();                                   \
    return vs_sai->get(                                 \
            entry,                                      \
            attr_count,                                 \
            attr_list);                                 \
}

// QUAD ENTRY DECLARE

#define VS_GENERIC_QUAD_ENTRY(OT,ot)    \
    VS_CREATE_ENTRY(OT,ot);             \
    VS_REMOVE_ENTRY(OT,ot);             \
    VS_SET_ENTRY(OT,ot);                \
    VS_GET_ENTRY(OT,ot);

// QUAD API

#define VS_GENERIC_QUAD_API(ot)     \
    vs_create_ ## ot,               \
    vs_remove_ ## ot,               \
    vs_set_ ## ot ##_attribute,     \
    vs_get_ ## ot ##_attribute,

// STATS

#define VS_GET_STATS(OT,ot)                             \
    static sai_status_t vs_get_ ## ot ## _stats(        \
            _In_ sai_object_id_t object_id,             \
            _In_ uint32_t number_of_counters,           \
            _In_ const sai_stat_id_t *counter_ids,      \
            _Out_ uint64_t *counters)                   \
{                                                       \
    SWSS_LOG_ENTER();                                   \
    return vs_sai->getStats(                            \
            (sai_object_type_t)SAI_OBJECT_TYPE_ ## OT,  \
            object_id,                                  \
            number_of_counters,                         \
            counter_ids,                                \
            counters);                                  \
}

#define VS_GET_STATS_EXT(OT,ot)                         \
    static sai_status_t vs_get_ ## ot ## _stats_ext(    \
            _In_ sai_object_id_t object_id,             \
            _In_ uint32_t number_of_counters,           \
            _In_ const sai_stat_id_t *counter_ids,      \
            _In_ sai_stats_mode_t mode,                 \
            _Out_ uint64_t *counters)                   \
{                                                       \
    SWSS_LOG_ENTER();                                   \
    return vs_sai->getStatsExt(                         \
            (sai_object_type_t)SAI_OBJECT_TYPE_ ## OT,  \
            object_id,                                  \
            number_of_counters,                         \
            counter_ids,                                \
            mode,                                       \
            counters);                                  \
}

#define VS_CLEAR_STATS(OT,ot)                           \
    static sai_status_t vs_clear_ ## ot ## _stats(      \
            _In_ sai_object_id_t object_id,             \
            _In_ uint32_t number_of_counters,           \
            _In_ const sai_stat_id_t *counter_ids)      \
{                                                       \
    SWSS_LOG_ENTER();                                   \
    return vs_sai->clearStats(                          \
            (sai_object_type_t)SAI_OBJECT_TYPE_ ## OT,  \
            object_id,                                  \
            number_of_counters,                         \
            counter_ids);                               \
}

// STATS DECLARE

#define VS_GENERIC_STATS(OT, ot)    \
    VS_GET_STATS(OT,ot);            \
    VS_GET_STATS_EXT(OT,ot);        \
    VS_CLEAR_STATS(OT,ot);

// STATS API

#define VS_GENERIC_STATS_API(ot)    \
    vs_get_ ## ot ## _stats,        \
    vs_get_ ## ot ## _stats_ext,    \
    vs_clear_ ## ot ## _stats,

// BULK QUAD

#define VS_BULK_CREATE(OT,fname)                    \
    static sai_status_t vs_bulk_create_ ## fname(   \
            _In_ sai_object_id_t switch_id,         \
            _In_ uint32_t object_count,             \
            _In_ const uint32_t *attr_count,        \
            _In_ const sai_attribute_t **attr_list, \
            _In_ sai_bulk_op_error_mode_t mode,     \
            _Out_ sai_object_id_t *object_id,       \
            _Out_ sai_status_t *object_statuses)    \
{                                                   \
    SWSS_LOG_ENTER();                               \
    return vs_sai->bulkCreate(                      \
            SAI_OBJECT_TYPE_ ## OT,                 \
            switch_id,                              \
            object_count,                           \
            attr_count,                             \
            attr_list,                              \
            mode,                                   \
            object_id,                              \
            object_statuses);                       \
}

#define VS_BULK_REMOVE(OT,fname)                    \
    static sai_status_t vs_bulk_remove_ ## fname(   \
            _In_ uint32_t object_count,             \
            _In_ const sai_object_id_t *object_id,  \
            _In_ sai_bulk_op_error_mode_t mode,     \
            _Out_ sai_status_t *object_statuses)    \
{                                                   \
    SWSS_LOG_ENTER();                               \
    return vs_sai->bulkRemove(                      \
            SAI_OBJECT_TYPE_ ## OT,                 \
            object_count,                           \
            object_id,                              \
            mode,                                   \
            object_statuses);                       \
}

#define VS_BULK_SET(OT,fname)                       \
    static sai_status_t vs_bulk_set_ ## fname(      \
            _In_ uint32_t object_count,             \
            _In_ const sai_object_id_t *object_id,  \
            _In_ const sai_attribute_t *attr_list,  \
            _In_ sai_bulk_op_error_mode_t mode,     \
            _Out_ sai_status_t *object_statuses)    \
{                                                   \
    SWSS_LOG_ENTER();                               \
    return vs_sai->bulkSet(                         \
            SAI_OBJECT_TYPE_ ## OT,                 \
            object_count,                           \
            object_id,                              \
            attr_list,                              \
            mode,                                   \
            object_statuses);                       \
}

#define VS_BULK_GET(OT,fname)                       \
    static sai_status_t vs_bulk_get_ ## fname(      \
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

#define VS_BULK_QUAD(OT,ot)     \
    VS_BULK_CREATE(OT,ot);      \
    VS_BULK_REMOVE(OT,ot);      \
    VS_BULK_SET(OT,ot);         \
    VS_BULK_GET(OT,ot);

// BULK QUAD ENTRY

#define VS_BULK_CREATE_ENTRY(OT,ot)                 \
    static sai_status_t vs_bulk_create_ ## ot(      \
            _In_ uint32_t object_count,             \
            _In_ const sai_ ## ot ## _t *entry,     \
            _In_ const uint32_t *attr_count,        \
            _In_ const sai_attribute_t **attr_list, \
            _In_ sai_bulk_op_error_mode_t mode,     \
            _Out_ sai_status_t *object_statuses)    \
{                                                   \
    SWSS_LOG_ENTER();                               \
    return vs_sai->bulkCreate(                      \
            object_count,                           \
            entry,                                  \
            attr_count,                             \
            attr_list,                              \
            mode,                                   \
            object_statuses);                       \
}

#define VS_BULK_REMOVE_ENTRY(OT,ot)                 \
    static sai_status_t vs_bulk_remove_ ## ot(      \
            _In_ uint32_t object_count,             \
            _In_ const sai_ ## ot ##_t *entry,      \
            _In_ sai_bulk_op_error_mode_t mode,     \
            _Out_ sai_status_t *object_statuses)    \
{                                                   \
    SWSS_LOG_ENTER();                               \
    return vs_sai->bulkRemove(                      \
            object_count,                           \
            entry,                                  \
            mode,                                   \
            object_statuses);                       \
}

#define VS_BULK_SET_ENTRY(OT,ot)                    \
    static sai_status_t vs_bulk_set_ ## ot(         \
            _In_ uint32_t object_count,             \
            _In_ const sai_ ## ot ## _t *entry,     \
            _In_ const sai_attribute_t *attr_list,  \
            _In_ sai_bulk_op_error_mode_t mode,     \
            _Out_ sai_status_t *object_statuses)    \
{                                                   \
    SWSS_LOG_ENTER();                               \
    return vs_sai->bulkSet(                         \
            object_count,                           \
            entry,                                  \
            attr_list,                              \
            mode,                                   \
            object_statuses);                       \
}

#define VS_BULK_GET_ENTRY(OT,ot)                    \
    static sai_status_t vs_bulk_get_ ## ot(         \
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

#define VS_BULK_QUAD_ENTRY(OT,ot)   \
    VS_BULK_CREATE_ENTRY(OT,ot);    \
    VS_BULK_REMOVE_ENTRY(OT,ot);    \
    VS_BULK_SET_ENTRY(OT,ot);       \
    VS_BULK_GET_ENTRY(OT,ot);

// BULK QUAD API

#define VS_BULK_QUAD_API(ot)        \
    vs_bulk_create_ ## ot,          \
    vs_bulk_remove_ ## ot,          \
    vs_bulk_set_ ## ot,             \
    vs_bulk_get_ ## ot,

