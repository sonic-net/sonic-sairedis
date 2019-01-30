#ifndef __SAI_REDIS__
#define __SAI_REDIS__

#include <mutex>
#include <set>
#include <unordered_map>

#include <stdio.h>

extern "C" {
#include "sai.h"
}

#include "sai_redis_internal.h"

#include "swss/redisclient.h"
#include "swss/dbconnector.h"
#include "swss/producertable.h"
#include "swss/consumertable.h"
#include "swss/notificationconsumer.h"
#include "swss/notificationproducer.h"
#include "swss/table.h"
#include "swss/select.h"
#include "swss/logger.h"
#include "meta/sai_meta.h"

/*
 * Switch index is encoded on 1 byte so we can have
 * max 0x100 switches at the same time.
 */

#define MAX_SWITCHES 0x100

void redis_clear_switch_ids();
void redis_free_virtual_object_id(
        _In_ sai_object_id_t object_id);

void clear_notifications();

void check_notifications_pointers(
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list);

// if we don't receive response from syncd in 60 seconds
// there is something wrong and we should fail
#define GET_RESPONSE_TIMEOUT (6*60*1000)

extern std::string getSelectResultAsString(int result);
extern void clear_local_state();
extern void setRecording(bool record);
extern sai_status_t setRecordingOutputDir(
        _In_ const sai_attribute_t &attr);
extern void recordLine(std::string s);
extern std::string joinFieldValues(
        _In_ const std::vector<swss::FieldValueTuple> &values);

// other global declarations

extern volatile bool g_record;
extern volatile bool g_useTempView;
extern volatile bool g_asicInitViewMode;
extern volatile bool g_logrotate;

extern sai_service_method_table_t                   g_services;
extern std::shared_ptr<swss::ProducerTable>         g_asicState;
extern std::shared_ptr<swss::ConsumerTable>         g_redisGetConsumer;
extern std::shared_ptr<swss::NotificationConsumer>  g_redisNotifications;
extern std::shared_ptr<swss::RedisClient>           g_redisClient;

extern std::mutex g_apimutex;

extern const sai_acl_api_t              redis_acl_api;
extern const sai_bfd_api_t              redis_bfd_api;
extern const sai_bmtor_api_t            redis_bmtor_api;
extern const sai_bridge_api_t           redis_bridge_api;
extern const sai_buffer_api_t           redis_buffer_api;
extern const sai_dtel_api_t             redis_dtel_api;
extern const sai_fdb_api_t              redis_fdb_api;
extern const sai_hash_api_t             redis_hash_api;
extern const sai_hostif_api_t           redis_hostif_api;
extern const sai_ipmc_api_t             redis_ipmc_api;
extern const sai_ipmc_group_api_t       redis_ipmc_group_api;
extern const sai_l2mc_api_t             redis_l2mc_api;
extern const sai_l2mc_group_api_t       redis_l2mc_group_api;
extern const sai_lag_api_t              redis_lag_api;
extern const sai_mcast_fdb_api_t        redis_mcast_fdb_api;
extern const sai_mirror_api_t           redis_mirror_api;
extern const sai_mpls_api_t             redis_mpls_api;
extern const sai_neighbor_api_t         redis_neighbor_api;
extern const sai_next_hop_api_t         redis_next_hop_api;
extern const sai_next_hop_group_api_t   redis_next_hop_group_api;
extern const sai_policer_api_t          redis_policer_api;
extern const sai_port_api_t             redis_port_api;
extern const sai_qos_map_api_t          redis_qos_map_api;
extern const sai_queue_api_t            redis_queue_api;
extern const sai_route_api_t            redis_route_api;
extern const sai_router_interface_api_t redis_router_interface_api;
extern const sai_rpf_group_api_t        redis_rpf_group_api;
extern const sai_samplepacket_api_t     redis_samplepacket_api;
extern const sai_scheduler_api_t        redis_scheduler_api;
extern const sai_scheduler_group_api_t  redis_scheduler_group_api;
extern const sai_segmentroute_api_t     redis_segmentroute_api;
extern const sai_stp_api_t              redis_stp_api;
extern const sai_switch_api_t           redis_switch_api;
extern const sai_tam_api_t              redis_tam_api;
extern const sai_tunnel_api_t           redis_tunnel_api;
extern const sai_uburst_api_t           redis_uburst_api;
extern const sai_udf_api_t              redis_udf_api;
extern const sai_virtual_router_api_t   redis_virtual_router_api;
extern const sai_vlan_api_t             redis_vlan_api;
extern const sai_wred_api_t             redis_wred_api;

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

// OID QUAD

sai_status_t redis_generic_create(
        _In_ sai_object_type_t object_type,
        _Out_ sai_object_id_t *object_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list);

sai_status_t redis_generic_remove(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id);

sai_status_t redis_generic_set(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ const sai_attribute_t *attr);

sai_status_t redis_generic_get(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t attr_count,
        _Out_ sai_attribute_t *attr_list);

// ENTRY QUAD

#define REDIS_CREATE_ENTRY_DEF(ot)                  \
    sai_status_t redis_generic_create_ ## ot(       \
            _In_ const sai_ ## ot ## _t * ot,       \
            _In_ uint32_t attr_count,               \
            _In_ const sai_attribute_t *attr_list);

#define REDIS_REMOVE_ENTRY_DEF(ot)                  \
    sai_status_t redis_generic_remove_ ## ot(       \
            _In_ const sai_ ## ot ## _t * ot);

#define REDIS_SET_ENTRY_DEF(ot)                     \
    sai_status_t redis_generic_set_ ## ot(          \
            _In_ const sai_ ## ot ## _t * ot,       \
            _In_ const sai_attribute_t *attr);

#define REDIS_GET_ENTRY_DEF(ot)                     \
    sai_status_t redis_generic_get_ ## ot(          \
            _In_ const sai_ ## ot ## _t * ot,       \
            _In_ uint32_t attr_count,               \
            _Out_ sai_attribute_t *attr_list);

#define REDIS_ENTRY_QUAD(ot)       \
    REDIS_CREATE_ENTRY_DEF(ot)     \
    REDIS_REMOVE_ENTRY_DEF(ot)     \
    REDIS_SET_ENTRY_DEF(ot)        \
    REDIS_GET_ENTRY_DEF(ot)

REDIS_ENTRY_QUAD(fdb_entry);
REDIS_ENTRY_QUAD(inseg_entry);
REDIS_ENTRY_QUAD(ipmc_entry);
REDIS_ENTRY_QUAD(l2mc_entry);
REDIS_ENTRY_QUAD(mcast_fdb_entry);
REDIS_ENTRY_QUAD(neighbor_entry);
REDIS_ENTRY_QUAD(route_entry);

// BULK

sai_status_t redis_bulk_generic_create(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t object_count,
        _Out_ sai_object_id_t *object_id, /* array */
        _In_ sai_object_id_t switch_id,
        _In_ const uint32_t *attr_count, /* array */
        _In_ const sai_attribute_t *const *attr_list, /* array */
        _Inout_ sai_status_t *object_statuses); /* array */

sai_status_t internal_redis_bulk_generic_create(
        _In_ sai_object_type_t object_type,
        _In_ const std::vector<std::string> &serialized_object_ids,
        _In_ const uint32_t *attr_count,
        _In_ const sai_attribute_t *const *attr_list, /* array */
        _Inout_ sai_status_t *object_statuses) /* array */;

sai_status_t redis_bulk_generic_remove(
        _In_ sai_object_type_t object_type,
        _In_ uint32_t object_count,
        _In_ const sai_object_id_t *object_id, /* array */
        _Inout_ sai_status_t *object_statuses) /* array */;

sai_status_t internal_redis_bulk_generic_remove(
        _In_ sai_object_type_t object_type,
        _In_ const std::vector<std::string> &serialized_object_ids,
        _Out_ sai_status_t *object_statuses); /* array */

sai_status_t internal_redis_bulk_generic_set(
        _In_ sai_object_type_t object_type,
        _In_ const std::vector<std::string> &serialized_object_ids,
        _In_ const sai_attribute_t *attr_list, /* array */
        _In_ const sai_status_t *object_statuses); /* array */

// get_stats

template <typename T>
sai_status_t redis_generic_get_stats(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ uint32_t count,
        _In_ const T* counter_id_list,
        _Out_ uint64_t *counter_list);

// notifications

void handle_notification(
        _In_ const std::string &notification,
        _In_ const std::string &data,
        _In_ const std::vector<swss::FieldValueTuple> &values);

// STATS

sai_status_t redis_generic_get_stats(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ const sai_enum_metadata_t *enum_metadata,
        _In_ uint32_t number_of_counters,
        _In_ const int32_t *counter_ids,
        _Out_ uint64_t *counters);

sai_status_t redis_generic_get_stats_ext(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ const sai_enum_metadata_t *enum_metadata,
        _In_ uint32_t number_of_counters,
        _In_ const int32_t *counter_ids,
        _In_ sai_stats_mode_t mode,
        _Out_ uint64_t *counters);

sai_status_t redis_generic_clear_stats(
        _In_ sai_object_type_t object_type,
        _In_ sai_object_id_t object_id,
        _In_ const sai_enum_metadata_t *enum_metadata,
        _In_ uint32_t number_of_counters,
        _In_ const int32_t *counter_ids);

#endif // __SAI_REDIS__
