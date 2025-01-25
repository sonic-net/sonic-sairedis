#pragma once

extern "C" {
#include "otai.h"
#include "otaimetadata.h"
}

// Curretnly empty in OTAI

#define OTAI_METADATA_DECLARE_EVERY_ENTRY(OTAI_USER_X_ENTRY_MACRO) ;

#define OTAI_METADATA_DECLARE_EVERY_BULK_ENTRY(OTAI_USER_X_BULK_ENTRY_MACRO) ;

#define OTAIREDIS_DECLARE_EVERY_ENTRY(_X)                                     \
    OTAI_METADATA_DECLARE_EVERY_ENTRY(_X)

#define OTAIREDIS_DECLARE_EVERY_BULK_ENTRY(_X)                                \
    OTAI_METADATA_DECLARE_EVERY_BULK_ENTRY(_X)

#define OTAIREDIS_OTAIINTERFACE_DECLARE_QUAD_ENTRY_VIRTUAL(OT,ot)     \
    virtual otai_status_t create(                                    \
            _In_ const otai_ ## ot ## _t* ot,                        \
            _In_ uint32_t attr_count,                               \
            _In_ const otai_attribute_t *attr_list) = 0;             \
    virtual otai_status_t remove(                                    \
            _In_ const otai_ ## ot ## _t* ot) = 0;                   \
    virtual otai_status_t set(                                       \
            _In_ const otai_ ## ot ## _t* ot,                        \
            _In_ const otai_attribute_t *attr) = 0;                  \
    virtual otai_status_t get(                                       \
            _In_ const otai_ ## ot ## _t* ot,                        \
            _In_ uint32_t attr_count,                               \
            _Out_ otai_attribute_t *attr_list) = 0;                  \

#define OTAIREDIS_OTAIINTERFACE_DECLARE_QUAD_ENTRY_OVERRIDE(OT,ot)    \
    virtual otai_status_t create(                                    \
            _In_ const otai_ ## ot ## _t* ot,                        \
            _In_ uint32_t attr_count,                               \
            _In_ const otai_attribute_t *attr_list) override;        \
    virtual otai_status_t remove(                                    \
            _In_ const otai_ ## ot ## _t* ot) override;              \
    virtual otai_status_t set(                                       \
            _In_ const otai_ ## ot ## _t* ot,                        \
            _In_ const otai_attribute_t *attr) override;             \
    virtual otai_status_t get(                                       \
            _In_ const otai_ ## ot ## _t* ot,                        \
            _In_ uint32_t attr_count,                               \
            _Out_ otai_attribute_t *attr_list) override;             \

#define OTAIREDIS_OTAIINTERFACE_DECLARE_BULK_ENTRY_VIRTUAL(OT,ot)     \
    virtual otai_status_t bulkCreate(                                \
            _In_ uint32_t object_count,                             \
            _In_ const otai_ ## ot ## _t *ot,                        \
            _In_ const uint32_t *attr_count,                        \
            _In_ const otai_attribute_t **attr_list,                 \
            _In_ otai_bulk_op_error_mode_t mode,                     \
            _Out_ otai_status_t *object_statuses) = 0;               \
    virtual otai_status_t bulkRemove(                                \
            _In_ uint32_t object_count,                             \
            _In_ const otai_ ## ot ## _t *ot,                        \
            _In_ otai_bulk_op_error_mode_t mode,                     \
            _Out_ otai_status_t *object_statuses) = 0;               \
    virtual otai_status_t bulkSet(                                   \
            _In_ uint32_t object_count,                             \
            _In_ const otai_ ## ot ## _t *ot,                        \
            _In_ const otai_attribute_t *attr_list,                  \
            _In_ otai_bulk_op_error_mode_t mode,                     \
            _Out_ otai_status_t *object_statuses) = 0;               \
    virtual otai_status_t bulkGet(                                   \
            _In_ uint32_t object_count,                             \
            _In_ const otai_ ## ot ## _t *ot,                        \
            _In_ const uint32_t *attr_count,                        \
            _Inout_ otai_attribute_t **attr_list,                    \
            _In_ otai_bulk_op_error_mode_t mode,                     \
            _Out_ otai_status_t *object_statuses) = 0;               \

#define OTAIREDIS_OTAIINTERFACE_DECLARE_BULK_ENTRY_OVERRIDE(OT,ot)    \
    virtual otai_status_t bulkCreate(                                \
            _In_ uint32_t object_count,                             \
            _In_ const otai_ ## ot ## _t *ot,                        \
            _In_ const uint32_t *attr_count,                        \
            _In_ const otai_attribute_t **attr_list,                 \
            _In_ otai_bulk_op_error_mode_t mode,                     \
            _Out_ otai_status_t *object_statuses) override;          \
    virtual otai_status_t bulkRemove(                                \
            _In_ uint32_t object_count,                             \
            _In_ const otai_ ## ot ## _t *ot,                        \
            _In_ otai_bulk_op_error_mode_t mode,                     \
            _Out_ otai_status_t *object_statuses) override;          \
    virtual otai_status_t bulkSet(                                   \
            _In_ uint32_t object_count,                             \
            _In_ const otai_ ## ot ## _t *ot,                        \
            _In_ const otai_attribute_t *attr_list,                  \
            _In_ otai_bulk_op_error_mode_t mode,                     \
            _Out_ otai_status_t *object_statuses) override;          \
    virtual otai_status_t bulkGet(                                   \
            _In_ uint32_t object_count,                             \
            _In_ const otai_ ## ot ## _t *ot,                        \
            _In_ const uint32_t *attr_count,                        \
            _Inout_ otai_attribute_t **attr_list,                    \
            _In_ otai_bulk_op_error_mode_t mode,                     \
            _Out_ otai_status_t *object_statuses) override;          \

namespace otairedis
{
    class OtaiInterface
    {
        public:

            OtaiInterface() = default;

            virtual ~OtaiInterface() = default;

        public:

            virtual otai_status_t apiInitialize(
                    _In_ uint64_t flags,
                    _In_ const otai_service_method_table_t *service_method_table) = 0;

            virtual otai_status_t apiUninitialize(void) = 0;

        public: // QUAD oid

            virtual otai_status_t create(
                    _In_ otai_object_type_t objectType,
                    _Out_ otai_object_id_t* objectId,
                    _In_ otai_object_id_t switchId,
                    _In_ uint32_t attr_count,
                    _In_ const otai_attribute_t *attr_list) = 0;

            virtual otai_status_t remove(
                    _In_ otai_object_type_t objectType,
                    _In_ otai_object_id_t objectId) = 0;

            virtual otai_status_t set(
                    _In_ otai_object_type_t objectType,
                    _In_ otai_object_id_t objectId,
                    _In_ const otai_attribute_t *attr) = 0;

            virtual otai_status_t get(
                    _In_ otai_object_type_t objectType,
                    _In_ otai_object_id_t objectId,
                    _In_ uint32_t attr_count,
                    _Inout_ otai_attribute_t *attr_list) = 0;

        public: // QUAD ENTRY and BULK QUAD ENTRY

            OTAIREDIS_DECLARE_EVERY_ENTRY(OTAIREDIS_OTAIINTERFACE_DECLARE_QUAD_ENTRY_VIRTUAL);
            OTAIREDIS_DECLARE_EVERY_BULK_ENTRY(OTAIREDIS_OTAIINTERFACE_DECLARE_BULK_ENTRY_VIRTUAL);

        public: // bulk QUAD oid

            virtual otai_status_t bulkCreate(
                    _In_ otai_object_type_t object_type,
                    _In_ otai_object_id_t switch_id,
                    _In_ uint32_t object_count,
                    _In_ const uint32_t *attr_count,
                    _In_ const otai_attribute_t **attr_list,
                    _In_ otai_bulk_op_error_mode_t mode,
                    _Out_ otai_object_id_t *object_id,
                    _Out_ otai_status_t *object_statuses) = 0;

            virtual otai_status_t bulkRemove(
                    _In_ otai_object_type_t object_type,
                    _In_ uint32_t object_count,
                    _In_ const otai_object_id_t *object_id,
                    _In_ otai_bulk_op_error_mode_t mode,
                    _Out_ otai_status_t *object_statuses) = 0;

            virtual otai_status_t bulkSet(
                    _In_ otai_object_type_t object_type,
                    _In_ uint32_t object_count,
                    _In_ const otai_object_id_t *object_id,
                    _In_ const otai_attribute_t *attr_list,
                    _In_ otai_bulk_op_error_mode_t mode,
                    _Out_ otai_status_t *object_statuses) = 0;

            virtual otai_status_t bulkGet(
                    _In_ otai_object_type_t object_type,
                    _In_ uint32_t object_count,
                    _In_ const otai_object_id_t *object_id,
                    _In_ const uint32_t *attr_count,
                    _Inout_ otai_attribute_t **attr_list,
                    _In_ otai_bulk_op_error_mode_t mode,
                    _Out_ otai_status_t *object_statuses) = 0;

        public: // QUAD meta key

            virtual otai_status_t create(
                    _Inout_ otai_object_meta_key_t& metaKey,
                    _In_ otai_object_id_t switch_id,
                    _In_ uint32_t attr_count,
                    _In_ const otai_attribute_t *attr_list);

            virtual otai_status_t remove(
                    _In_ const otai_object_meta_key_t& metaKey);

            virtual otai_status_t set(
                    _In_ const otai_object_meta_key_t& metaKey,
                    _In_ const otai_attribute_t *attr);

            virtual otai_status_t get(
                    _In_ const otai_object_meta_key_t& metaKey,
                    _In_ uint32_t attr_count,
                    _Inout_ otai_attribute_t *attr_list);

        public: // stats API

            virtual otai_status_t getStats(
                    _In_ otai_object_type_t object_type,
                    _In_ otai_object_id_t object_id,
                    _In_ uint32_t number_of_counters,
                    _In_ const otai_stat_id_t *counter_ids,
                    _Out_ uint64_t *counters) = 0;

            //virtual otai_status_t queryStatsCapability(
            //        _In_ otai_object_id_t switch_id,
            //        _In_ otai_object_type_t object_type,
            //        _Inout_ otai_stat_capability_list_t *stats_capability) = 0;

            virtual otai_status_t getStatsExt(
                    _In_ otai_object_type_t object_type,
                    _In_ otai_object_id_t object_id,
                    _In_ uint32_t number_of_counters,
                    _In_ const otai_stat_id_t *counter_ids,
                    _In_ otai_stats_mode_t mode,
                    _Out_ uint64_t *counters) = 0;

            virtual otai_status_t clearStats(
                    _In_ otai_object_type_t object_type,
                    _In_ otai_object_id_t object_id,
                    _In_ uint32_t number_of_counters,
                    _In_ const otai_stat_id_t *counter_ids) = 0;

            virtual otai_status_t bulkGetStats(
                    _In_ otai_object_id_t switchId,
                    _In_ otai_object_type_t object_type,
                    _In_ uint32_t object_count,
                    _In_ const otai_object_key_t *object_key,
                    _In_ uint32_t number_of_counters,
                    _In_ const otai_stat_id_t *counter_ids,
                    _In_ otai_stats_mode_t mode,
                    _Inout_ otai_status_t *object_statuses,
                    _Out_ uint64_t *counters) = 0;

            virtual otai_status_t bulkClearStats(
                    _In_ otai_object_id_t switchId,
                    _In_ otai_object_type_t object_type,
                    _In_ uint32_t object_count,
                    _In_ const otai_object_key_t *object_key,
                    _In_ uint32_t number_of_counters,
                    _In_ const otai_stat_id_t *counter_ids,
                    _In_ otai_stats_mode_t mode,
                    _Inout_ otai_status_t *object_statuses) = 0;

        public: // entry stats

            //virtual otai_status_t getStats(
            //        _In_ const otai_meter_bucket_entry_t* entry,
            //        _In_ uint32_t number_of_counters,
            //        _In_ const otai_stat_id_t *counter_ids,
            //        _Out_ uint64_t *counters);

            //virtual otai_status_t getStatsExt(
            //        _In_ const otai_meter_bucket_entry_t* entry,
            //        _In_ uint32_t number_of_counters,
            //        _In_ const otai_stat_id_t *counter_ids,
            //        _In_ otai_stats_mode_t mode,
            //        _Out_ uint64_t *counters);

            //virtual otai_status_t clearStats(
            //        _In_ const otai_meter_bucket_entry_t* entry,
            //        _In_ uint32_t number_of_counters,
            //        _In_ const otai_stat_id_t *counter_ids);

        public: // non QUAD API

            virtual otai_status_t flushFdbEntries(
                    _In_ otai_object_id_t switchId,
                    _In_ uint32_t attrCount,
                    _In_ const otai_attribute_t *attrList) = 0;

            virtual otai_status_t switchMdioRead(
                    _In_ otai_object_id_t switch_id,
                    _In_ uint32_t device_addr,
                    _In_ uint32_t start_reg_addr,
                    _In_ uint32_t number_of_registers,
                    _Out_ uint32_t *reg_val);

            virtual otai_status_t switchMdioWrite(
                    _In_ otai_object_id_t switch_id,
                    _In_ uint32_t device_addr,
                    _In_ uint32_t start_reg_addr,
                    _In_ uint32_t number_of_registers,
                    _In_ const uint32_t *reg_val);

            virtual otai_status_t switchMdioCl22Read(
                    _In_ otai_object_id_t switch_id,
                    _In_ uint32_t device_addr,
                    _In_ uint32_t start_reg_addr,
                    _In_ uint32_t number_of_registers,
                    _Out_ uint32_t *reg_val);

            virtual otai_status_t switchMdioCl22Write(
                    _In_ otai_object_id_t switch_id,
                    _In_ uint32_t device_addr,
                    _In_ uint32_t start_reg_addr,
                    _In_ uint32_t number_of_registers,
                    _In_ const uint32_t *reg_val);

        public: // OTAI API

            virtual otai_status_t objectTypeGetAvailability(
                    _In_ otai_object_id_t switchId,
                    _In_ otai_object_type_t objectType,
                    _In_ uint32_t attrCount,
                    _In_ const otai_attribute_t *attrList,
                    _Out_ uint64_t *count) = 0;

            //virtual otai_status_t queryAttributeCapability(
            //        _In_ otai_object_id_t switch_id,
            //        _In_ otai_object_type_t object_type,
            //        _In_ otai_attr_id_t attr_id,
            //        _Out_ otai_attr_capability_t *capability) = 0;

            virtual otai_status_t queryAttributeEnumValuesCapability(
                    _In_ otai_object_id_t switch_id,
                    _In_ otai_object_type_t object_type,
                    _In_ otai_attr_id_t attr_id,
                    _Inout_ otai_s32_list_t *enum_values_capability) = 0;

            virtual otai_object_type_t objectTypeQuery(
                    _In_ otai_object_id_t objectId) = 0;

            virtual otai_object_id_t switchIdQuery(
                    _In_ otai_object_id_t objectId) = 0;

            virtual otai_status_t logSet(
                    _In_ otai_api_t api,
                    _In_ otai_log_level_t log_level) = 0;

            //virtual otai_status_t queryApiVersion(
            //        _Out_ otai_api_version_t *version) = 0;

        public: // non OTAI API

            virtual otai_log_level_t logGet(
                    _In_ otai_api_t api);
    };
}
