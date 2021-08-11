#pragma once

extern "C" {
#include "sai.h"
#include "saimetadata.h"
}

//#include "sairedis.h"

#define SAIREDIS_SAIINTERFACE_DECLARE_REMOVE_ENTRY(ot)  \
    virtual sai_status_t remove(                        \
            _In_ const sai_ ## ot ## _t* ot) = 0;

#define SAIREDIS_SAIINTERFACE_DECLARE_CREATE_ENTRY(ot)  \
    virtual sai_status_t create(                        \
            _In_ const sai_ ## ot ## _t* ot,            \
            _In_ uint32_t attr_count,                   \
            _In_ const sai_attribute_t *attr_list) = 0;

#define SAIREDIS_SAIINTERFACE_DECLARE_SET_ENTRY(ot)     \
    virtual sai_status_t set(                           \
            _In_ const sai_ ## ot ## _t* ot,            \
            _In_ const sai_attribute_t *attr) = 0;

#define SAIREDIS_SAIINTERFACE_DECLARE_GET_ENTRY(ot)     \
    virtual sai_status_t get(                           \
            _In_ const sai_ ## ot ## _t* ot,            \
            _In_ uint32_t attr_count,                   \
            _Out_ sai_attribute_t *attr_list) = 0;      \

#define SAIREDIS_SAIINTERFACE_DECLARE_BULK_CREATE_ENTRY(ot) \
    virtual sai_status_t bulkCreate(                        \
            _In_ uint32_t object_count,                     \
            _In_ const sai_ ## ot ## _t *ot,                \
            _In_ const uint32_t *attr_count,                \
            _In_ const sai_attribute_t **attr_list,         \
            _In_ sai_bulk_op_error_mode_t mode,             \
            _Out_ sai_status_t *object_statuses) = 0;

#define SAIREDIS_SAIINTERFACE_DECLARE_BULK_REMOVE_ENTRY(ot) \
    virtual sai_status_t bulkRemove(                        \
            _In_ uint32_t object_count,                     \
            _In_ const sai_ ## ot ## _t *ot,                \
            _In_ sai_bulk_op_error_mode_t mode,             \
            _Out_ sai_status_t *object_statuses) = 0;

#define SAIREDIS_SAIINTERFACE_DECLARE_BULK_SET_ENTRY(ot)    \
    virtual sai_status_t bulkSet(                           \
            _In_ uint32_t object_count,                     \
            _In_ const sai_ ## ot ## _t *ot,                \
            _In_ const sai_attribute_t *attr_list,          \
            _In_ sai_bulk_op_error_mode_t mode,             \
            _Out_ sai_status_t *object_statuses) = 0;

namespace sairedis
{
    class SaiInterface
    {
        public:

            SaiInterface() = default;

            virtual ~SaiInterface() = default;

        public:

            virtual sai_status_t initialize(
                    _In_ uint64_t flags,
                    _In_ const sai_service_method_table_t *service_method_table) = 0;

            virtual sai_status_t uninitialize(void) = 0;

        public: // QUAD oid

            virtual sai_status_t create(
                    _In_ sai_object_type_t objectType,
                    _Out_ sai_object_id_t* objectId,
                    _In_ sai_object_id_t switchId,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list) = 0;

            virtual sai_status_t remove(
                    _In_ sai_object_type_t objectType,
                    _In_ sai_object_id_t objectId) = 0;

            virtual sai_status_t set(
                    _In_ sai_object_type_t objectType,
                    _In_ sai_object_id_t objectId,
                    _In_ const sai_attribute_t *attr) = 0;

            virtual sai_status_t get(
                    _In_ sai_object_type_t objectType,
                    _In_ sai_object_id_t objectId,
                    _In_ uint32_t attr_count,
                    _Inout_ sai_attribute_t *attr_list) = 0;

        public: // create ENTRY

            SAIREDIS_SAIINTERFACE_DECLARE_CREATE_ENTRY(fdb_entry);
            SAIREDIS_SAIINTERFACE_DECLARE_CREATE_ENTRY(inseg_entry);
            SAIREDIS_SAIINTERFACE_DECLARE_CREATE_ENTRY(ipmc_entry);
            SAIREDIS_SAIINTERFACE_DECLARE_CREATE_ENTRY(l2mc_entry);
            SAIREDIS_SAIINTERFACE_DECLARE_CREATE_ENTRY(mcast_fdb_entry);
            SAIREDIS_SAIINTERFACE_DECLARE_CREATE_ENTRY(neighbor_entry);
            SAIREDIS_SAIINTERFACE_DECLARE_CREATE_ENTRY(route_entry);
            SAIREDIS_SAIINTERFACE_DECLARE_CREATE_ENTRY(nat_entry);

        public: // remove ENTRY

            SAIREDIS_SAIINTERFACE_DECLARE_REMOVE_ENTRY(fdb_entry);
            SAIREDIS_SAIINTERFACE_DECLARE_REMOVE_ENTRY(inseg_entry);
            SAIREDIS_SAIINTERFACE_DECLARE_REMOVE_ENTRY(ipmc_entry);
            SAIREDIS_SAIINTERFACE_DECLARE_REMOVE_ENTRY(l2mc_entry);
            SAIREDIS_SAIINTERFACE_DECLARE_REMOVE_ENTRY(mcast_fdb_entry);
            SAIREDIS_SAIINTERFACE_DECLARE_REMOVE_ENTRY(neighbor_entry);
            SAIREDIS_SAIINTERFACE_DECLARE_REMOVE_ENTRY(route_entry);
            SAIREDIS_SAIINTERFACE_DECLARE_REMOVE_ENTRY(nat_entry);

        public: // set ENTRY

            SAIREDIS_SAIINTERFACE_DECLARE_SET_ENTRY(fdb_entry);
            SAIREDIS_SAIINTERFACE_DECLARE_SET_ENTRY(inseg_entry);
            SAIREDIS_SAIINTERFACE_DECLARE_SET_ENTRY(ipmc_entry);
            SAIREDIS_SAIINTERFACE_DECLARE_SET_ENTRY(l2mc_entry);
            SAIREDIS_SAIINTERFACE_DECLARE_SET_ENTRY(mcast_fdb_entry);
            SAIREDIS_SAIINTERFACE_DECLARE_SET_ENTRY(neighbor_entry);
            SAIREDIS_SAIINTERFACE_DECLARE_SET_ENTRY(route_entry);
            SAIREDIS_SAIINTERFACE_DECLARE_SET_ENTRY(nat_entry);

        public: // get ENTRY

            SAIREDIS_SAIINTERFACE_DECLARE_GET_ENTRY(fdb_entry);
            SAIREDIS_SAIINTERFACE_DECLARE_GET_ENTRY(inseg_entry);
            SAIREDIS_SAIINTERFACE_DECLARE_GET_ENTRY(ipmc_entry);
            SAIREDIS_SAIINTERFACE_DECLARE_GET_ENTRY(l2mc_entry);
            SAIREDIS_SAIINTERFACE_DECLARE_GET_ENTRY(mcast_fdb_entry);
            SAIREDIS_SAIINTERFACE_DECLARE_GET_ENTRY(neighbor_entry);
            SAIREDIS_SAIINTERFACE_DECLARE_GET_ENTRY(route_entry);
            SAIREDIS_SAIINTERFACE_DECLARE_GET_ENTRY(nat_entry);

        public: // bulk QUAD oid

            virtual sai_status_t bulkCreate(
                    _In_ sai_object_type_t object_type,
                    _In_ sai_object_id_t switch_id,
                    _In_ uint32_t object_count,
                    _In_ const uint32_t *attr_count,
                    _In_ const sai_attribute_t **attr_list,
                    _In_ sai_bulk_op_error_mode_t mode,
                    _Out_ sai_object_id_t *object_id,
                    _Out_ sai_status_t *object_statuses) = 0;

            virtual sai_status_t bulkRemove(
                    _In_ sai_object_type_t object_type,
                    _In_ uint32_t object_count,
                    _In_ const sai_object_id_t *object_id,
                    _In_ sai_bulk_op_error_mode_t mode,
                    _Out_ sai_status_t *object_statuses) = 0;

            virtual sai_status_t bulkSet(
                    _In_ sai_object_type_t object_type,
                    _In_ uint32_t object_count,
                    _In_ const sai_object_id_t *object_id,
                    _In_ const sai_attribute_t *attr_list,
                    _In_ sai_bulk_op_error_mode_t mode,
                    _Out_ sai_status_t *object_statuses) = 0;

        public: // bulk create ENTRY

            SAIREDIS_SAIINTERFACE_DECLARE_BULK_CREATE_ENTRY(fdb_entry);
            SAIREDIS_SAIINTERFACE_DECLARE_BULK_CREATE_ENTRY(inseg_entry);
            SAIREDIS_SAIINTERFACE_DECLARE_BULK_CREATE_ENTRY(nat_entry);
            SAIREDIS_SAIINTERFACE_DECLARE_BULK_CREATE_ENTRY(route_entry);

        public: // bulk remove ENTRY

            SAIREDIS_SAIINTERFACE_DECLARE_BULK_REMOVE_ENTRY(fdb_entry);
            SAIREDIS_SAIINTERFACE_DECLARE_BULK_REMOVE_ENTRY(inseg_entry);
            SAIREDIS_SAIINTERFACE_DECLARE_BULK_REMOVE_ENTRY(nat_entry);
            SAIREDIS_SAIINTERFACE_DECLARE_BULK_REMOVE_ENTRY(route_entry);

        public: // bulk set ENTRY

            SAIREDIS_SAIINTERFACE_DECLARE_BULK_SET_ENTRY(fdb_entry);
            SAIREDIS_SAIINTERFACE_DECLARE_BULK_SET_ENTRY(inseg_entry);
            SAIREDIS_SAIINTERFACE_DECLARE_BULK_SET_ENTRY(nat_entry);
            SAIREDIS_SAIINTERFACE_DECLARE_BULK_SET_ENTRY(route_entry);

        public: // QUAD meta key

            virtual sai_status_t create(
                    _Inout_ sai_object_meta_key_t& metaKey,
                    _In_ sai_object_id_t switch_id,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

            virtual sai_status_t remove(
                    _In_ const sai_object_meta_key_t& metaKey);

            virtual sai_status_t set(
                    _In_ const sai_object_meta_key_t& metaKey,
                    _In_ const sai_attribute_t *attr);

            virtual sai_status_t get(
                    _In_ const sai_object_meta_key_t& metaKey,
                    _In_ uint32_t attr_count,
                    _Inout_ sai_attribute_t *attr_list);

        public: // stats API

            virtual sai_status_t getStats(
                    _In_ sai_object_type_t object_type,
                    _In_ sai_object_id_t object_id,
                    _In_ uint32_t number_of_counters,
                    _In_ const sai_stat_id_t *counter_ids,
                    _Out_ uint64_t *counters) = 0;

            virtual sai_status_t getStatsExt(
                    _In_ sai_object_type_t object_type,
                    _In_ sai_object_id_t object_id,
                    _In_ uint32_t number_of_counters,
                    _In_ const sai_stat_id_t *counter_ids,
                    _In_ sai_stats_mode_t mode,
                    _Out_ uint64_t *counters) = 0;

            virtual sai_status_t clearStats(
                    _In_ sai_object_type_t object_type,
                    _In_ sai_object_id_t object_id,
                    _In_ uint32_t number_of_counters,
                    _In_ const sai_stat_id_t *counter_ids) = 0;

        public: // non QUAD API

            virtual sai_status_t flushFdbEntries(
                    _In_ sai_object_id_t switchId,
                    _In_ uint32_t attrCount,
                    _In_ const sai_attribute_t *attrList) = 0;

        public: // SAI API

            virtual sai_status_t objectTypeGetAvailability(
                    _In_ sai_object_id_t switchId,
                    _In_ sai_object_type_t objectType,
                    _In_ uint32_t attrCount,
                    _In_ const sai_attribute_t *attrList,
                    _Out_ uint64_t *count) = 0;

            virtual sai_status_t queryAttributeCapability(
                    _In_ sai_object_id_t switch_id,
                    _In_ sai_object_type_t object_type,
                    _In_ sai_attr_id_t attr_id,
                    _Out_ sai_attr_capability_t *capability) = 0;

            virtual sai_status_t queryAattributeEnumValuesCapability(
                    _In_ sai_object_id_t switch_id,
                    _In_ sai_object_type_t object_type,
                    _In_ sai_attr_id_t attr_id,
                    _Inout_ sai_s32_list_t *enum_values_capability) = 0;

            virtual sai_object_type_t objectTypeQuery(
                    _In_ sai_object_id_t objectId) = 0;

            virtual sai_object_id_t switchIdQuery(
                    _In_ sai_object_id_t objectId) = 0;

            virtual sai_status_t logSet(
                    _In_ sai_api_t api,
                    _In_ sai_log_level_t log_level) = 0;
    };
}
