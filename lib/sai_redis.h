#pragma once

extern "C" {
#include "sai.h"
#include "saiextensions.h"
}

#include "meta/SaiInterface.h"

#include "swss/logger.h"

#include <memory>

#define PRIVATE __attribute__((visibility("hidden")))

PRIVATE extern const sai_acl_api_t              redis_acl_api;
PRIVATE extern const sai_bfd_api_t              redis_bfd_api;
PRIVATE extern const sai_bmtor_api_t            redis_bmtor_api;
PRIVATE extern const sai_bridge_api_t           redis_bridge_api;
PRIVATE extern const sai_buffer_api_t           redis_buffer_api;
PRIVATE extern const sai_counter_api_t          redis_counter_api;
PRIVATE extern const sai_debug_counter_api_t    redis_debug_counter_api;
PRIVATE extern const sai_dtel_api_t             redis_dtel_api;
PRIVATE extern const sai_fdb_api_t              redis_fdb_api;
PRIVATE extern const sai_hash_api_t             redis_hash_api;
PRIVATE extern const sai_hostif_api_t           redis_hostif_api;
PRIVATE extern const sai_ipmc_api_t             redis_ipmc_api;
PRIVATE extern const sai_ipmc_group_api_t       redis_ipmc_group_api;
PRIVATE extern const sai_isolation_group_api_t  redis_isolation_group_api;
PRIVATE extern const sai_l2mc_api_t             redis_l2mc_api;
PRIVATE extern const sai_l2mc_group_api_t       redis_l2mc_group_api;
PRIVATE extern const sai_lag_api_t              redis_lag_api;
PRIVATE extern const sai_macsec_api_t           redis_macsec_api;
PRIVATE extern const sai_mcast_fdb_api_t        redis_mcast_fdb_api;
PRIVATE extern const sai_mirror_api_t           redis_mirror_api;
PRIVATE extern const sai_mpls_api_t             redis_mpls_api;
PRIVATE extern const sai_nat_api_t              redis_nat_api;
PRIVATE extern const sai_neighbor_api_t         redis_neighbor_api;
PRIVATE extern const sai_next_hop_api_t         redis_next_hop_api;
PRIVATE extern const sai_next_hop_group_api_t   redis_next_hop_group_api;
PRIVATE extern const sai_policer_api_t          redis_policer_api;
PRIVATE extern const sai_port_api_t             redis_port_api;
PRIVATE extern const sai_qos_map_api_t          redis_qos_map_api;
PRIVATE extern const sai_queue_api_t            redis_queue_api;
PRIVATE extern const sai_route_api_t            redis_route_api;
PRIVATE extern const sai_router_interface_api_t redis_router_interface_api;
PRIVATE extern const sai_rpf_group_api_t        redis_rpf_group_api;
PRIVATE extern const sai_samplepacket_api_t     redis_samplepacket_api;
PRIVATE extern const sai_scheduler_api_t        redis_scheduler_api;
PRIVATE extern const sai_scheduler_group_api_t  redis_scheduler_group_api;
PRIVATE extern const sai_srv6_api_t             redis_srv6_api;
PRIVATE extern const sai_stp_api_t              redis_stp_api;
PRIVATE extern const sai_switch_api_t           redis_switch_api;
PRIVATE extern const sai_system_port_api_t      redis_system_port_api;
PRIVATE extern const sai_tam_api_t              redis_tam_api;
PRIVATE extern const sai_tunnel_api_t           redis_tunnel_api;
PRIVATE extern const sai_udf_api_t              redis_udf_api;
PRIVATE extern const sai_virtual_router_api_t   redis_virtual_router_api;
PRIVATE extern const sai_vlan_api_t             redis_vlan_api;
PRIVATE extern const sai_wred_api_t             redis_wred_api;
PRIVATE extern const sai_my_mac_api_t           redis_my_mac_api;
PRIVATE extern const sai_ipsec_api_t            redis_ipsec_api;

PRIVATE extern std::shared_ptr<sairedis::SaiInterface>   redis_sai;

// QUAD OID

#define REDIS_CREATE(OT,ot)                             \
    static sai_status_t redis_create_ ## ot(            \
            _Out_ sai_object_id_t *object_id,           \
            _In_ sai_object_id_t switch_id,             \
            _In_ uint32_t attr_count,                   \
            _In_ const sai_attribute_t *attr_list)      \
{                                                       \
    SWSS_LOG_ENTER();                                   \
    return redis_sai->create(                           \
            (sai_object_type_t)SAI_OBJECT_TYPE_ ## OT,  \
            object_id,                                  \
            switch_id,                                  \
            attr_count,                                 \
            attr_list);                                 \
}

#define REDIS_REMOVE(OT,ot)                             \
    static sai_status_t redis_remove_ ## ot(            \
            _In_ sai_object_id_t object_id)             \
{                                                       \
    SWSS_LOG_ENTER();                                   \
    return redis_sai->remove(                           \
            (sai_object_type_t)SAI_OBJECT_TYPE_ ## OT,  \
            object_id);                                 \
}

#define REDIS_SET(OT,ot)                                \
    static sai_status_t redis_set_ ## ot ## _attribute( \
            _In_ sai_object_id_t object_id,             \
            _In_ const sai_attribute_t *attr)           \
{                                                       \
    SWSS_LOG_ENTER();                                   \
    return redis_sai->set(                              \
            (sai_object_type_t)SAI_OBJECT_TYPE_ ## OT,  \
            object_id,                                  \
            attr);                                      \
}

#define REDIS_GET(OT,ot)                                \
    static sai_status_t redis_get_ ## ot ## _attribute( \
            _In_ sai_object_id_t object_id,             \
            _In_ uint32_t attr_count,                   \
            _Inout_ sai_attribute_t *attr_list)         \
{                                                       \
    SWSS_LOG_ENTER();                                   \
    return redis_sai->get(                              \
            (sai_object_type_t)SAI_OBJECT_TYPE_ ## OT,  \
            object_id,                                  \
            attr_count,                                 \
            attr_list);                                 \
}

// QUAD DECLARE

#define REDIS_GENERIC_QUAD(OT,ot)  \
    REDIS_CREATE(OT,ot);           \
    REDIS_REMOVE(OT,ot);           \
    REDIS_SET(OT,ot);              \
    REDIS_GET(OT,ot);

// QUAD ENTRY

#define REDIS_CREATE_ENTRY(OT,ot)                       \
    static sai_status_t redis_create_ ## ot(            \
            _In_ const sai_ ## ot ##_t *entry,          \
            _In_ uint32_t attr_count,                   \
            _In_ const sai_attribute_t *attr_list)      \
{                                                       \
    SWSS_LOG_ENTER();                                   \
    return redis_sai->create(                           \
            entry,                                      \
            attr_count,                                 \
            attr_list);                                 \
}

#define REDIS_REMOVE_ENTRY(OT,ot)                       \
    static sai_status_t redis_remove_ ## ot(            \
            _In_ const sai_ ## ot ## _t *entry)         \
{                                                       \
    SWSS_LOG_ENTER();                                   \
    return redis_sai->remove(                           \
            entry);                                     \
}

#define REDIS_SET_ENTRY(OT,ot)                          \
    static sai_status_t redis_set_ ## ot ## _attribute( \
            _In_ const sai_ ## ot ## _t *entry,         \
            _In_ const sai_attribute_t *attr)           \
{                                                       \
    SWSS_LOG_ENTER();                                   \
    return redis_sai->set(                              \
            entry,                                      \
            attr);                                      \
}

#define REDIS_GET_ENTRY(OT,ot)                          \
    static sai_status_t redis_get_ ## ot ## _attribute( \
            _In_ const sai_ ## ot ## _t *entry,         \
            _In_ uint32_t attr_count,                   \
            _Inout_ sai_attribute_t *attr_list)         \
{                                                       \
    SWSS_LOG_ENTER();                                   \
    return redis_sai->get(                              \
            entry,                                      \
            attr_count,                                 \
            attr_list);                                 \
}

// QUAD ENTRY DECLARE

#define REDIS_GENERIC_QUAD_ENTRY(OT,ot)    \
    REDIS_CREATE_ENTRY(OT,ot);             \
    REDIS_REMOVE_ENTRY(OT,ot);             \
    REDIS_SET_ENTRY(OT,ot);                \
    REDIS_GET_ENTRY(OT,ot);

// QUAD API

#define REDIS_GENERIC_QUAD_API(ot)      \
    redis_create_ ## ot,                \
    redis_remove_ ## ot,                \
    redis_set_ ## ot ##_attribute,      \
    redis_get_ ## ot ##_attribute,

// STATS

#define REDIS_GET_STATS(OT,ot)                          \
    static sai_status_t redis_get_ ## ot ## _stats(     \
            _In_ sai_object_id_t object_id,             \
            _In_ uint32_t number_of_counters,           \
            _In_ const sai_stat_id_t *counter_ids,      \
            _Out_ uint64_t *counters)                   \
{                                                       \
    SWSS_LOG_ENTER();                                   \
    return redis_sai->getStats(                         \
            (sai_object_type_t)SAI_OBJECT_TYPE_ ## OT,  \
            object_id,                                  \
            number_of_counters,                         \
            counter_ids,                                \
            counters);                                  \
}

#define REDIS_GET_STATS_EXT(OT,ot)                      \
    static sai_status_t redis_get_ ## ot ## _stats_ext( \
            _In_ sai_object_id_t object_id,             \
            _In_ uint32_t number_of_counters,           \
            _In_ const sai_stat_id_t *counter_ids,      \
            _In_ sai_stats_mode_t mode,                 \
            _Out_ uint64_t *counters)                   \
{                                                       \
    SWSS_LOG_ENTER();                                   \
    return redis_sai->getStatsExt(                      \
            (sai_object_type_t)SAI_OBJECT_TYPE_ ## OT,  \
            object_id,                                  \
            number_of_counters,                         \
            counter_ids,                                \
            mode,                                       \
            counters);                                  \
}

#define REDIS_CLEAR_STATS(OT,ot)                        \
    static sai_status_t redis_clear_ ## ot ## _stats(   \
            _In_ sai_object_id_t object_id,             \
            _In_ uint32_t number_of_counters,           \
            _In_ const sai_stat_id_t *counter_ids)      \
{                                                       \
    SWSS_LOG_ENTER();                                   \
    return redis_sai->clearStats(                       \
            (sai_object_type_t)SAI_OBJECT_TYPE_ ## OT,  \
            object_id,                                  \
            number_of_counters,                         \
            counter_ids);                               \
}

// STATS DECLARE

#define REDIS_GENERIC_STATS(OT, ot)    \
    REDIS_GET_STATS(OT,ot);            \
    REDIS_GET_STATS_EXT(OT,ot);        \
    REDIS_CLEAR_STATS(OT,ot);

// STATS API

#define REDIS_GENERIC_STATS_API(ot)     \
    redis_get_ ## ot ## _stats,         \
    redis_get_ ## ot ## _stats_ext,     \
    redis_clear_ ## ot ## _stats,

// BULK QUAD

#define REDIS_BULK_CREATE(OT,fname)                 \
    static sai_status_t redis_bulk_create_ ## fname(\
            _In_ sai_object_id_t switch_id,         \
            _In_ uint32_t object_count,             \
            _In_ const uint32_t *attr_count,        \
            _In_ const sai_attribute_t **attr_list, \
            _In_ sai_bulk_op_error_mode_t mode,     \
            _Out_ sai_object_id_t *object_id,       \
            _Out_ sai_status_t *object_statuses)    \
{                                                   \
    SWSS_LOG_ENTER();                               \
    return redis_sai->bulkCreate(                   \
            SAI_OBJECT_TYPE_ ## OT,                 \
            switch_id,                              \
            object_count,                           \
            attr_count,                             \
            attr_list,                              \
            mode,                                   \
            object_id,                              \
            object_statuses);                       \
}

#define REDIS_BULK_REMOVE(OT,fname)                 \
    static sai_status_t redis_bulk_remove_ ## fname(\
            _In_ uint32_t object_count,             \
            _In_ const sai_object_id_t *object_id,  \
            _In_ sai_bulk_op_error_mode_t mode,     \
            _Out_ sai_status_t *object_statuses)    \
{                                                   \
    SWSS_LOG_ENTER();                               \
    return redis_sai->bulkRemove(                   \
            SAI_OBJECT_TYPE_ ## OT,                 \
            object_count,                           \
            object_id,                              \
            mode,                                   \
            object_statuses);                       \
}

#define REDIS_BULK_SET(OT,fname)                    \
    static sai_status_t redis_bulk_set_ ## fname(   \
            _In_ uint32_t object_count,             \
            _In_ const sai_object_id_t *object_id,  \
            _In_ const sai_attribute_t *attr_list,  \
            _In_ sai_bulk_op_error_mode_t mode,     \
            _Out_ sai_status_t *object_statuses)    \
{                                                   \
    SWSS_LOG_ENTER();                               \
    return redis_sai->bulkSet(                      \
            SAI_OBJECT_TYPE_ ## OT,                 \
            object_count,                           \
            object_id,                              \
            attr_list,                              \
            mode,                                   \
            object_statuses);                       \
}

#define REDIS_BULK_GET(OT,fname)                    \
    static sai_status_t redis_bulk_get_ ## fname(   \
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

#define REDIS_BULK_QUAD(OT,ot)     \
    REDIS_BULK_CREATE(OT,ot);      \
    REDIS_BULK_REMOVE(OT,ot);      \
    REDIS_BULK_SET(OT,ot);         \
    REDIS_BULK_GET(OT,ot);

// BULK QUAD ENTRY

#define REDIS_BULK_CREATE_ENTRY(OT,ot)              \
    static sai_status_t redis_bulk_create_ ## ot(   \
            _In_ uint32_t object_count,             \
            _In_ const sai_ ## ot ## _t *entry,     \
            _In_ const uint32_t *attr_count,        \
            _In_ const sai_attribute_t **attr_list, \
            _In_ sai_bulk_op_error_mode_t mode,     \
            _Out_ sai_status_t *object_statuses)    \
{                                                   \
    SWSS_LOG_ENTER();                               \
    return redis_sai->bulkCreate(                   \
            object_count,                           \
            entry,                                  \
            attr_count,                             \
            attr_list,                              \
            mode,                                   \
            object_statuses);                       \
}

#define REDIS_BULK_REMOVE_ENTRY(OT,ot)              \
    static sai_status_t redis_bulk_remove_ ## ot(   \
            _In_ uint32_t object_count,             \
            _In_ const sai_ ## ot ##_t *entry,      \
            _In_ sai_bulk_op_error_mode_t mode,     \
            _Out_ sai_status_t *object_statuses)    \
{                                                   \
    SWSS_LOG_ENTER();                               \
    return redis_sai->bulkRemove(                   \
            object_count,                           \
            entry,                                  \
            mode,                                   \
            object_statuses);                       \
}

#define REDIS_BULK_SET_ENTRY(OT,ot)                 \
    static sai_status_t redis_bulk_set_ ## ot(      \
            _In_ uint32_t object_count,             \
            _In_ const sai_ ## ot ## _t *entry,     \
            _In_ const sai_attribute_t *attr_list,  \
            _In_ sai_bulk_op_error_mode_t mode,     \
            _Out_ sai_status_t *object_statuses)    \
{                                                   \
    SWSS_LOG_ENTER();                               \
    return redis_sai->bulkSet(                      \
            object_count,                           \
            entry,                                  \
            attr_list,                              \
            mode,                                   \
            object_statuses);                       \
}

#define REDIS_BULK_GET_ENTRY(OT,ot)                 \
    static sai_status_t redis_bulk_get_ ## ot(      \
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

#define REDIS_BULK_QUAD_ENTRY(OT,ot)   \
    REDIS_BULK_CREATE_ENTRY(OT,ot);    \
    REDIS_BULK_REMOVE_ENTRY(OT,ot);    \
    REDIS_BULK_SET_ENTRY(OT,ot);       \
    REDIS_BULK_GET_ENTRY(OT,ot);

// BULK QUAD API

#define REDIS_BULK_QUAD_API(ot)     \
    redis_bulk_create_ ## ot,       \
    redis_bulk_remove_ ## ot,       \
    redis_bulk_set_ ## ot,          \
    redis_bulk_get_ ## ot,

// BULK get/set DECLARE

#define REDIS_BULK_GET_SET(OT,ot)   \
    REDIS_BULK_GET(OT,ot);          \
    REDIS_BULK_SET(OT,ot);

// BULK get/set API

#define REDIS_BULK_GET_SET_API(ot)     \
    redis_bulk_get_ ## ot,             \
    redis_bulk_set_ ## ot,
