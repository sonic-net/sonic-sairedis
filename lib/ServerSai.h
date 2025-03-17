#pragma once

#include "meta/SaiInterface.h"
#include "meta/SaiAttributeList.h"
#include "meta/SelectableChannel.h"

#include "swss/selectableevent.h"

#include <memory>
#include <mutex>
#include <thread>

namespace sairedis
{
    class ServerSai:
        public sairedis::SaiInterface
    {
        public:

            ServerSai();

            virtual ~ServerSai();

        public:

            sai_status_t apiInitialize(
                    _In_ uint64_t flags,
                    _In_ const sai_service_method_table_t *service_method_table) override;

            sai_status_t apiUninitialize(void) override;

        public: // SAI interface overrides

            virtual sai_status_t create(
                    _In_ sai_object_type_t objectType,
                    _Out_ sai_object_id_t* objectId,
                    _In_ sai_object_id_t switchId,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list) override;

            virtual sai_status_t remove(
                    _In_ sai_object_type_t objectType,
                    _In_ sai_object_id_t objectId) override;

            virtual sai_status_t set(
                    _In_ sai_object_type_t objectType,
                    _In_ sai_object_id_t objectId,
                    _In_ const sai_attribute_t *attr) override;

            virtual sai_status_t get(
                    _In_ sai_object_type_t objectType,
                    _In_ sai_object_id_t objectId,
                    _In_ uint32_t attr_count,
                    _Inout_ sai_attribute_t *attr_list) override;

        public: // QUAD ENTRY and BULK QUAD ENTRY

            SAIREDIS_DECLARE_EVERY_ENTRY(SAIREDIS_SAIINTERFACE_DECLARE_QUAD_ENTRY_OVERRIDE);
            SAIREDIS_DECLARE_EVERY_BULK_ENTRY(SAIREDIS_SAIINTERFACE_DECLARE_BULK_ENTRY_OVERRIDE);

        public: // bulk QUAD oid

            virtual sai_status_t bulkCreate(
                    _In_ sai_object_type_t object_type,
                    _In_ sai_object_id_t switch_id,
                    _In_ uint32_t object_count,
                    _In_ const uint32_t *attr_count,
                    _In_ const sai_attribute_t **attr_list,
                    _In_ sai_bulk_op_error_mode_t mode,
                    _Out_ sai_object_id_t *object_id,
                    _Out_ sai_status_t *object_statuses) override;

            virtual sai_status_t bulkRemove(
                    _In_ sai_object_type_t object_type,
                    _In_ uint32_t object_count,
                    _In_ const sai_object_id_t *object_id,
                    _In_ sai_bulk_op_error_mode_t mode,
                    _Out_ sai_status_t *object_statuses) override;

            virtual sai_status_t bulkSet(
                    _In_ sai_object_type_t object_type,
                    _In_ uint32_t object_count,
                    _In_ const sai_object_id_t *object_id,
                    _In_ const sai_attribute_t *attr_list,
                    _In_ sai_bulk_op_error_mode_t mode,
                    _Out_ sai_status_t *object_statuses) override;

            virtual sai_status_t bulkGet(
                    _In_ sai_object_type_t object_type,
                    _In_ uint32_t object_count,
                    _In_ const sai_object_id_t *object_id,
                    _In_ const uint32_t *attr_count,
                    _Inout_ sai_attribute_t **attr_list,
                    _In_ sai_bulk_op_error_mode_t mode,
                    _Out_ sai_status_t *object_statuses) override;

        public: // stats API

            virtual sai_status_t getStats(
                    _In_ sai_object_type_t object_type,
                    _In_ sai_object_id_t object_id,
                    _In_ uint32_t number_of_counters,
                    _In_ const sai_stat_id_t *counter_ids,
                    _Out_ uint64_t *counters) override;

            virtual sai_status_t queryStatsCapability(
                    _In_ sai_object_id_t switch_id,
                    _In_ sai_object_type_t object_type,
                    _Inout_ sai_stat_capability_list_t *stats_capability) override;

            virtual sai_status_t queryStatsStCapability(
                    _In_ sai_object_id_t switch_id,
                    _In_ sai_object_type_t object_type,
                    _Inout_ sai_stat_st_capability_list_t *stats_capability) override;

            virtual sai_status_t getStatsExt(
                    _In_ sai_object_type_t object_type,
                    _In_ sai_object_id_t object_id,
                    _In_ uint32_t number_of_counters,
                    _In_ const sai_stat_id_t *counter_ids,
                    _In_ sai_stats_mode_t mode,
                    _Out_ uint64_t *counters) override;

            virtual sai_status_t clearStats(
                    _In_ sai_object_type_t object_type,
                    _In_ sai_object_id_t object_id,
                    _In_ uint32_t number_of_counters,
                    _In_ const sai_stat_id_t *counter_ids) override;

            virtual sai_status_t bulkGetStats(
                    _In_ sai_object_id_t switchId,
                    _In_ sai_object_type_t object_type,
                    _In_ uint32_t object_count,
                    _In_ const sai_object_key_t *object_key,
                    _In_ uint32_t number_of_counters,
                    _In_ const sai_stat_id_t *counter_ids,
                    _In_ sai_stats_mode_t mode,
                    _Inout_ sai_status_t *object_statuses,
                    _Out_ uint64_t *counters) override;

            virtual sai_status_t bulkClearStats(
                    _In_ sai_object_id_t switchId,
                    _In_ sai_object_type_t object_type,
                    _In_ uint32_t object_count,
                    _In_ const sai_object_key_t *object_key,
                    _In_ uint32_t number_of_counters,
                    _In_ const sai_stat_id_t *counter_ids,
                    _In_ sai_stats_mode_t mode,
                    _Inout_ sai_status_t *object_statuses) override;

        public: // non QUAD API

            virtual sai_status_t flushFdbEntries(
                    _In_ sai_object_id_t switchId,
                    _In_ uint32_t attrCount,
                    _In_ const sai_attribute_t *attrList) override;

        public: // SAI API

            virtual sai_status_t objectTypeGetAvailability(
                    _In_ sai_object_id_t switchId,
                    _In_ sai_object_type_t objectType,
                    _In_ uint32_t attrCount,
                    _In_ const sai_attribute_t *attrList,
                    _Out_ uint64_t *count) override;

            virtual sai_status_t queryAttributeCapability(
                    _In_ sai_object_id_t switch_id,
                    _In_ sai_object_type_t object_type,
                    _In_ sai_attr_id_t attr_id,
                    _Out_ sai_attr_capability_t *capability) override;

            virtual sai_status_t queryAttributeEnumValuesCapability(
                    _In_ sai_object_id_t switch_id,
                    _In_ sai_object_type_t object_type,
                    _In_ sai_attr_id_t attr_id,
                    _Inout_ sai_s32_list_t *enum_values_capability) override;

            virtual sai_object_type_t objectTypeQuery(
                    _In_ sai_object_id_t objectId) override;

            virtual sai_object_id_t switchIdQuery(
                    _In_ sai_object_id_t objectId) override;

            virtual sai_status_t logSet(
                    _In_ sai_api_t api,
                    _In_ sai_log_level_t log_level) override;

            virtual sai_status_t queryApiVersion(
                    _Out_ sai_api_version_t *version) override;

        private:

            void serverThreadFunction();

            void processEvent(
                    _In_ SelectableChannel& consumer);

            sai_status_t processSingleEvent(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            // QUAD API

            sai_status_t processQuadEvent(
                    _In_ sai_common_api_t api,
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            sai_status_t processEntry(
                    _In_ sai_object_meta_key_t metaKey,
                    _In_ sai_common_api_t api,
                    _In_ uint32_t attr_count,
                    _In_ sai_attribute_t *attr_list);

            sai_status_t processOid(
                    _In_ sai_object_type_t objectType,
                    _Inout_ sai_object_id_t& oid,
                    _In_ sai_object_id_t switchId,
                    _In_ sai_common_api_t api,
                    _In_ uint32_t attr_count,
                    _In_ sai_attribute_t *attr_list);

            void sendApiResponse(
                    _In_ sai_common_api_t api,
                    _In_ sai_status_t status,
                    _In_ sai_object_id_t oid);

            void sendGetResponse(
                    _In_ sai_object_type_t objectType,
                    _In_ const std::string& strObjectId,
                    _In_ sai_status_t status,
                    _In_ uint32_t attr_count,
                    _In_ sai_attribute_t *attr_list);

            // BULK API

            sai_status_t processBulkQuadEvent(
                    _In_ sai_common_api_t api,
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            sai_status_t processBulkOid(
                    _In_ sai_object_type_t objectType,
                    _In_ const std::vector<std::string>& strObjectIds,
                    _In_ sai_common_api_t api,
                    _In_ const std::vector<std::shared_ptr<saimeta::SaiAttributeList>>& attributes,
                    _In_ const std::vector<std::vector<swss::FieldValueTuple>>& strAttributes);

            sai_status_t processBulkEntry(
                    _In_ sai_object_type_t objectType,
                    _In_ const std::vector<std::string>& objectIds,
                    _In_ sai_common_api_t api,
                    _In_ const std::vector<std::shared_ptr<saimeta::SaiAttributeList>>& attributes,
                    _In_ const std::vector<std::vector<swss::FieldValueTuple>>& strAttributes);

            sai_status_t processBulkCreateEntry(
                    _In_ sai_object_type_t objectType,
                    _In_ const std::vector<std::string>& objectIds,
                    _In_ const std::vector<std::shared_ptr<saimeta::SaiAttributeList>>& attributes,
                    _Out_ std::vector<sai_status_t>& statuses);

            sai_status_t processBulkRemoveEntry(
                    _In_ sai_object_type_t objectType,
                    _In_ const std::vector<std::string>& objectIds,
                    _Out_ std::vector<sai_status_t>& statuses);

            sai_status_t processBulkSetEntry(
                    _In_ sai_object_type_t objectType,
                    _In_ const std::vector<std::string>& objectIds,
                    _In_ const std::vector<std::shared_ptr<saimeta::SaiAttributeList>>& attributes,
                    _Out_ std::vector<sai_status_t>& statuses);

            void sendBulkApiResponse(
                    _In_ sai_common_api_t api,
                    _In_ sai_status_t status,
                    _In_ uint32_t object_count,
                    _In_ const sai_object_id_t* object_ids,
                    _In_ const sai_status_t* statuses);

            // STATS API

            sai_status_t processGetStatsEvent(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            sai_status_t processClearStatsEvent(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            // NON QUAD API

            sai_status_t processFdbFlush(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            // QUERY API

            sai_status_t processAttrCapabilityQuery(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            sai_status_t processAttrEnumValuesCapabilityQuery(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            sai_status_t processObjectTypeGetAvailabilityQuery(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

        private:

            bool m_apiInitialized;

            bool m_runServerThread;

            std::recursive_mutex m_apimutex;

            sai_service_method_table_t m_service_method_table;


            std::shared_ptr<std::thread> m_serverThread;


            swss::SelectableEvent m_serverThreadThreadShouldEndEvent;

        protected:

            sai_status_t processStatsCapabilityQuery(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            std::shared_ptr<SelectableChannel> m_selectableChannel;

            std::shared_ptr<SaiInterface> m_sai;
    };
}
