#pragma once

#include "meta/OtaiInterface.h"
#include "meta/OtaiAttributeList.h"
#include "meta/SelectableChannel.h"

#include "swss/selectableevent.h"

#include <memory>
#include <mutex>
#include <thread>

namespace otairedis
{
    class ServerOtai:
        public otairedis::OtaiInterface
    {
        public:

            ServerOtai();

            virtual ~ServerOtai();

        public:

            otai_status_t apiInitialize(
                    _In_ uint64_t flags,
                    _In_ const otai_service_method_table_t *service_method_table) override;

            otai_status_t apiUninitialize(void) override;

        public: // OTAI interface overrides

            virtual otai_status_t create(
                    _In_ otai_object_type_t objectType,
                    _Out_ otai_object_id_t* objectId,
                    _In_ otai_object_id_t switchId,
                    _In_ uint32_t attr_count,
                    _In_ const otai_attribute_t *attr_list) override;

            virtual otai_status_t remove(
                    _In_ otai_object_type_t objectType,
                    _In_ otai_object_id_t objectId) override;

            virtual otai_status_t set(
                    _In_ otai_object_type_t objectType,
                    _In_ otai_object_id_t objectId,
                    _In_ const otai_attribute_t *attr) override;

            virtual otai_status_t get(
                    _In_ otai_object_type_t objectType,
                    _In_ otai_object_id_t objectId,
                    _In_ uint32_t attr_count,
                    _Inout_ otai_attribute_t *attr_list) override;

        public: // QUAD ENTRY and BULK QUAD ENTRY

            OTAIREDIS_DECLARE_EVERY_ENTRY(OTAIREDIS_OTAIINTERFACE_DECLARE_QUAD_ENTRY_OVERRIDE);
            OTAIREDIS_DECLARE_EVERY_BULK_ENTRY(OTAIREDIS_OTAIINTERFACE_DECLARE_BULK_ENTRY_OVERRIDE);

        public: // bulk QUAD oid

            virtual otai_status_t bulkCreate(
                    _In_ otai_object_type_t object_type,
                    _In_ otai_object_id_t switch_id,
                    _In_ uint32_t object_count,
                    _In_ const uint32_t *attr_count,
                    _In_ const otai_attribute_t **attr_list,
                    _In_ otai_bulk_op_error_mode_t mode,
                    _Out_ otai_object_id_t *object_id,
                    _Out_ otai_status_t *object_statuses) override;

            virtual otai_status_t bulkRemove(
                    _In_ otai_object_type_t object_type,
                    _In_ uint32_t object_count,
                    _In_ const otai_object_id_t *object_id,
                    _In_ otai_bulk_op_error_mode_t mode,
                    _Out_ otai_status_t *object_statuses) override;

            virtual otai_status_t bulkSet(
                    _In_ otai_object_type_t object_type,
                    _In_ uint32_t object_count,
                    _In_ const otai_object_id_t *object_id,
                    _In_ const otai_attribute_t *attr_list,
                    _In_ otai_bulk_op_error_mode_t mode,
                    _Out_ otai_status_t *object_statuses) override;

            virtual otai_status_t bulkGet(
                    _In_ otai_object_type_t object_type,
                    _In_ uint32_t object_count,
                    _In_ const otai_object_id_t *object_id,
                    _In_ const uint32_t *attr_count,
                    _Inout_ otai_attribute_t **attr_list,
                    _In_ otai_bulk_op_error_mode_t mode,
                    _Out_ otai_status_t *object_statuses) override;

        public: // stats API

            virtual otai_status_t getStats(
                    _In_ otai_object_type_t object_type,
                    _In_ otai_object_id_t object_id,
                    _In_ uint32_t number_of_counters,
                    _In_ const otai_stat_id_t *counter_ids,
                    _Out_ uint64_t *counters) override;


            virtual otai_status_t getStatsExt(
                    _In_ otai_object_type_t object_type,
                    _In_ otai_object_id_t object_id,
                    _In_ uint32_t number_of_counters,
                    _In_ const otai_stat_id_t *counter_ids,
                    _In_ otai_stats_mode_t mode,
                    _Out_ uint64_t *counters) override;

            virtual otai_status_t clearStats(
                    _In_ otai_object_type_t object_type,
                    _In_ otai_object_id_t object_id,
                    _In_ uint32_t number_of_counters,
                    _In_ const otai_stat_id_t *counter_ids) override;

            virtual otai_status_t bulkGetStats(
                    _In_ otai_object_id_t switchId,
                    _In_ otai_object_type_t object_type,
                    _In_ uint32_t object_count,
                    _In_ const otai_object_key_t *object_key,
                    _In_ uint32_t number_of_counters,
                    _In_ const otai_stat_id_t *counter_ids,
                    _In_ otai_stats_mode_t mode,
                    _Inout_ otai_status_t *object_statuses,
                    _Out_ uint64_t *counters) override;

            virtual otai_status_t bulkClearStats(
                    _In_ otai_object_id_t switchId,
                    _In_ otai_object_type_t object_type,
                    _In_ uint32_t object_count,
                    _In_ const otai_object_key_t *object_key,
                    _In_ uint32_t number_of_counters,
                    _In_ const otai_stat_id_t *counter_ids,
                    _In_ otai_stats_mode_t mode,
                    _Inout_ otai_status_t *object_statuses) override;

        public: // non QUAD API

            virtual otai_status_t flushFdbEntries(
                    _In_ otai_object_id_t switchId,
                    _In_ uint32_t attrCount,
                    _In_ const otai_attribute_t *attrList) override;

        public: // OTAI API

            virtual otai_status_t objectTypeGetAvailability(
                    _In_ otai_object_id_t switchId,
                    _In_ otai_object_type_t objectType,
                    _In_ uint32_t attrCount,
                    _In_ const otai_attribute_t *attrList,
                    _Out_ uint64_t *count) override;

            virtual otai_status_t queryAttributeEnumValuesCapability(
                    _In_ otai_object_id_t switch_id,
                    _In_ otai_object_type_t object_type,
                    _In_ otai_attr_id_t attr_id,
                    _Inout_ otai_s32_list_t *enum_values_capability) override;

            virtual otai_object_type_t objectTypeQuery(
                    _In_ otai_object_id_t objectId) override;

            virtual otai_object_id_t switchIdQuery(
                    _In_ otai_object_id_t objectId) override;

            virtual otai_status_t logSet(
                    _In_ otai_api_t api,
                    _In_ otai_log_level_t log_level) override;


        private:

            void serverThreadFunction();

            void processEvent(
                    _In_ SelectableChannel& consumer);

            otai_status_t processSingleEvent(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            // QUAD API

            otai_status_t processQuadEvent(
                    _In_ otai_common_api_t api,
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            otai_status_t processEntry(
                    _In_ otai_object_meta_key_t metaKey,
                    _In_ otai_common_api_t api,
                    _In_ uint32_t attr_count,
                    _In_ otai_attribute_t *attr_list);

            otai_status_t processOid(
                    _In_ otai_object_type_t objectType,
                    _Inout_ otai_object_id_t& oid,
                    _In_ otai_object_id_t switchId,
                    _In_ otai_common_api_t api,
                    _In_ uint32_t attr_count,
                    _In_ otai_attribute_t *attr_list);

            void sendApiResponse(
                    _In_ otai_common_api_t api,
                    _In_ otai_status_t status,
                    _In_ otai_object_id_t oid);

            void sendGetResponse(
                    _In_ otai_object_type_t objectType,
                    _In_ const std::string& strObjectId,
                    _In_ otai_status_t status,
                    _In_ uint32_t attr_count,
                    _In_ otai_attribute_t *attr_list);

            // BULK API

            otai_status_t processBulkQuadEvent(
                    _In_ otai_common_api_t api,
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            otai_status_t processBulkOid(
                    _In_ otai_object_type_t objectType,
                    _In_ const std::vector<std::string>& strObjectIds,
                    _In_ otai_common_api_t api,
                    _In_ const std::vector<std::shared_ptr<otaimeta::OtaiAttributeList>>& attributes,
                    _In_ const std::vector<std::vector<swss::FieldValueTuple>>& strAttributes);

            otai_status_t processBulkEntry(
                    _In_ otai_object_type_t objectType,
                    _In_ const std::vector<std::string>& objectIds,
                    _In_ otai_common_api_t api,
                    _In_ const std::vector<std::shared_ptr<otaimeta::OtaiAttributeList>>& attributes,
                    _In_ const std::vector<std::vector<swss::FieldValueTuple>>& strAttributes);

            otai_status_t processBulkCreateEntry(
                    _In_ otai_object_type_t objectType,
                    _In_ const std::vector<std::string>& objectIds,
                    _In_ const std::vector<std::shared_ptr<otaimeta::OtaiAttributeList>>& attributes,
                    _Out_ std::vector<otai_status_t>& statuses);

            otai_status_t processBulkRemoveEntry(
                    _In_ otai_object_type_t objectType,
                    _In_ const std::vector<std::string>& objectIds,
                    _Out_ std::vector<otai_status_t>& statuses);

            otai_status_t processBulkSetEntry(
                    _In_ otai_object_type_t objectType,
                    _In_ const std::vector<std::string>& objectIds,
                    _In_ const std::vector<std::shared_ptr<otaimeta::OtaiAttributeList>>& attributes,
                    _Out_ std::vector<otai_status_t>& statuses);

            void sendBulkApiResponse(
                    _In_ otai_common_api_t api,
                    _In_ otai_status_t status,
                    _In_ uint32_t object_count,
                    _In_ const otai_object_id_t* object_ids,
                    _In_ const otai_status_t* statuses);

            // STATS API

            otai_status_t processGetStatsEvent(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            otai_status_t processClearStatsEvent(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            // NON QUAD API

            otai_status_t processFdbFlush(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            // QUERY API

            otai_status_t processAttrCapabilityQuery(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            otai_status_t processAttrEnumValuesCapabilityQuery(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            otai_status_t processObjectTypeGetAvailabilityQuery(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

        private:

            bool m_apiInitialized;

            bool m_runServerThread;

            std::recursive_mutex m_apimutex;

            otai_service_method_table_t m_service_method_table;

            std::shared_ptr<OtaiInterface> m_otai;

            std::shared_ptr<std::thread> m_serverThread;

            std::shared_ptr<SelectableChannel> m_selectableChannel;

            swss::SelectableEvent m_serverThreadThreadShouldEndEvent;
    };
}
