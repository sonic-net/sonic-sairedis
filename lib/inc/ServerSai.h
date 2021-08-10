#pragma once

#include "meta/SaiInterface.h"
#include "meta/SaiAttributeList.h"
#include "syncd/SelectableChannel.h"

#include "swss/selectableevent.h"

#include <memory>
#include <mutex>
#include <thread>

#define SAIREDIS_SERVERSAI_DECLARE_REMOVE_ENTRY(ot)                 \
    virtual sai_status_t remove(                                    \
            _In_ const sai_ ## ot ## _t* ot) override;

#define SAIREDIS_SERVERSAI_DECLARE_CREATE_ENTRY(ot)                 \
    virtual sai_status_t create(                                    \
            _In_ const sai_ ## ot ## _t* ot,                        \
            _In_ uint32_t attr_count,                               \
            _In_ const sai_attribute_t *attr_list) override;

#define SAIREDIS_SERVERSAI_DECLARE_SET_ENTRY(ot)                    \
    virtual sai_status_t set(                                       \
            _In_ const sai_ ## ot ## _t* ot,                        \
            _In_ const sai_attribute_t *attr) override;

#define SAIREDIS_SERVERSAI_DECLARE_GET_ENTRY(ot)                    \
    virtual sai_status_t get(                                       \
            _In_ const sai_ ## ot ## _t* ot,                        \
            _In_ uint32_t attr_count,                               \
            _Out_ sai_attribute_t *attr_list) override;

#define SAIREDIS_SERVERSAI_DECLARE_BULK_CREATE_ENTRY(ot)            \
    virtual sai_status_t bulkCreate(                                \
            _In_ uint32_t object_count,                             \
            _In_ const sai_ ## ot ## _t *ot,                        \
            _In_ const uint32_t *attr_count,                        \
            _In_ const sai_attribute_t **attr_list,                 \
            _In_ sai_bulk_op_error_mode_t mode,                     \
            _Out_ sai_status_t *object_statuses) override;

#define SAIREDIS_SERVERSAI_DECLARE_BULK_REMOVE_ENTRY(ot)            \
    virtual sai_status_t bulkRemove(                                \
            _In_ uint32_t object_count,                             \
            _In_ const sai_ ## ot ## _t *ot,                        \
            _In_ sai_bulk_op_error_mode_t mode,                     \
            _Out_ sai_status_t *object_statuses) override;

#define SAIREDIS_SERVERSAI_DECLARE_BULK_SET_ENTRY(ot)               \
    virtual sai_status_t bulkSet(                                   \
            _In_ uint32_t object_count,                             \
            _In_ const sai_ ## ot ## _t *ot,                        \
            _In_ const sai_attribute_t *attr_list,                  \
            _In_ sai_bulk_op_error_mode_t mode,                     \
            _Out_ sai_status_t *object_statuses) override;

namespace sairedis
{
    class ServerSai:
        public sairedis::SaiInterface
    {
        public:

            ServerSai();

            virtual ~ServerSai();

        public:

            sai_status_t initialize(
                    _In_ uint64_t flags,
                    _In_ const sai_service_method_table_t *service_method_table) override;

            sai_status_t uninitialize(void) override;

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

        public: // create ENTRY

            SAIREDIS_SERVERSAI_DECLARE_CREATE_ENTRY(fdb_entry);
            SAIREDIS_SERVERSAI_DECLARE_CREATE_ENTRY(inseg_entry);
            SAIREDIS_SERVERSAI_DECLARE_CREATE_ENTRY(ipmc_entry);
            SAIREDIS_SERVERSAI_DECLARE_CREATE_ENTRY(l2mc_entry);
            SAIREDIS_SERVERSAI_DECLARE_CREATE_ENTRY(mcast_fdb_entry);
            SAIREDIS_SERVERSAI_DECLARE_CREATE_ENTRY(neighbor_entry);
            SAIREDIS_SERVERSAI_DECLARE_CREATE_ENTRY(route_entry);
            SAIREDIS_SERVERSAI_DECLARE_CREATE_ENTRY(nat_entry);

        public: // remove ENTRY

            SAIREDIS_SERVERSAI_DECLARE_REMOVE_ENTRY(fdb_entry);
            SAIREDIS_SERVERSAI_DECLARE_REMOVE_ENTRY(inseg_entry);
            SAIREDIS_SERVERSAI_DECLARE_REMOVE_ENTRY(ipmc_entry);
            SAIREDIS_SERVERSAI_DECLARE_REMOVE_ENTRY(l2mc_entry);
            SAIREDIS_SERVERSAI_DECLARE_REMOVE_ENTRY(mcast_fdb_entry);
            SAIREDIS_SERVERSAI_DECLARE_REMOVE_ENTRY(neighbor_entry);
            SAIREDIS_SERVERSAI_DECLARE_REMOVE_ENTRY(route_entry);
            SAIREDIS_SERVERSAI_DECLARE_REMOVE_ENTRY(nat_entry);

        public: // set ENTRY

            SAIREDIS_SERVERSAI_DECLARE_SET_ENTRY(fdb_entry);
            SAIREDIS_SERVERSAI_DECLARE_SET_ENTRY(inseg_entry);
            SAIREDIS_SERVERSAI_DECLARE_SET_ENTRY(ipmc_entry);
            SAIREDIS_SERVERSAI_DECLARE_SET_ENTRY(l2mc_entry);
            SAIREDIS_SERVERSAI_DECLARE_SET_ENTRY(mcast_fdb_entry);
            SAIREDIS_SERVERSAI_DECLARE_SET_ENTRY(neighbor_entry);
            SAIREDIS_SERVERSAI_DECLARE_SET_ENTRY(route_entry);
            SAIREDIS_SERVERSAI_DECLARE_SET_ENTRY(nat_entry);

        public: // get ENTRY

            SAIREDIS_SERVERSAI_DECLARE_GET_ENTRY(fdb_entry);
            SAIREDIS_SERVERSAI_DECLARE_GET_ENTRY(inseg_entry);
            SAIREDIS_SERVERSAI_DECLARE_GET_ENTRY(ipmc_entry);
            SAIREDIS_SERVERSAI_DECLARE_GET_ENTRY(l2mc_entry);
            SAIREDIS_SERVERSAI_DECLARE_GET_ENTRY(mcast_fdb_entry);
            SAIREDIS_SERVERSAI_DECLARE_GET_ENTRY(neighbor_entry);
            SAIREDIS_SERVERSAI_DECLARE_GET_ENTRY(route_entry);
            SAIREDIS_SERVERSAI_DECLARE_GET_ENTRY(nat_entry);

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

        public: // bulk create ENTRY

            SAIREDIS_SERVERSAI_DECLARE_BULK_CREATE_ENTRY(fdb_entry);
            SAIREDIS_SERVERSAI_DECLARE_BULK_CREATE_ENTRY(inseg_entry);
            SAIREDIS_SERVERSAI_DECLARE_BULK_CREATE_ENTRY(nat_entry);
            SAIREDIS_SERVERSAI_DECLARE_BULK_CREATE_ENTRY(route_entry);

        public: // bulk remove ENTRY

            SAIREDIS_SERVERSAI_DECLARE_BULK_REMOVE_ENTRY(fdb_entry);
            SAIREDIS_SERVERSAI_DECLARE_BULK_REMOVE_ENTRY(inseg_entry);
            SAIREDIS_SERVERSAI_DECLARE_BULK_REMOVE_ENTRY(nat_entry);
            SAIREDIS_SERVERSAI_DECLARE_BULK_REMOVE_ENTRY(route_entry);

        public: // bulk set ENTRY

            SAIREDIS_SERVERSAI_DECLARE_BULK_SET_ENTRY(fdb_entry);
            SAIREDIS_SERVERSAI_DECLARE_BULK_SET_ENTRY(inseg_entry);
            SAIREDIS_SERVERSAI_DECLARE_BULK_SET_ENTRY(nat_entry);
            SAIREDIS_SERVERSAI_DECLARE_BULK_SET_ENTRY(route_entry);

        public: // stats API

            virtual sai_status_t getStats(
                    _In_ sai_object_type_t object_type,
                    _In_ sai_object_id_t object_id,
                    _In_ uint32_t number_of_counters,
                    _In_ const sai_stat_id_t *counter_ids,
                    _Out_ uint64_t *counters) override;

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

            virtual sai_status_t queryAattributeEnumValuesCapability(
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

        private:

            void serverThreadFunction();

            void processEvent(
                    _In_ syncd::SelectableChannel& consumer);

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

            std::shared_ptr<SaiInterface> m_sai;

            std::shared_ptr<std::thread> m_serverThread;

            std::shared_ptr<syncd::SelectableChannel> m_selectableChannel;

            swss::SelectableEvent m_serverThreadThreadShouldEndEvent;
    };
}
