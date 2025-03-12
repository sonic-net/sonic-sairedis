#pragma once

#include "RemoteSaiInterface.h"
#include "SwitchContainer.h"
#include "VirtualObjectIdManager.h"
#include "Recorder.h"
#include "RedisVidIndexGenerator.h"
#include "SkipRecordAttrContainer.h"
#include "RedisChannel.h"
#include "SwitchConfigContainer.h"
#include "ContextConfig.h"

#include "meta/Notification.h"

#include "swss/producertable.h"
#include "swss/consumertable.h"
#include "swss/notificationconsumer.h"
#include "swss/selectableevent.h"

#include <memory>
#include <functional>
#include <map>

namespace sairedis
{
    class RedisRemoteSaiInterface:
        public RemoteSaiInterface
    {
        public:

            RedisRemoteSaiInterface(
                    _In_ std::shared_ptr<ContextConfig> contextConfig,
                    _In_ std::function<sai_switch_notifications_t(std::shared_ptr<Notification>)> notificationCallback,
                    _In_ std::shared_ptr<Recorder> recorder);

            virtual ~RedisRemoteSaiInterface();

        public:

            virtual sai_status_t apiInitialize(
                    _In_ uint64_t flags,
                    _In_ const sai_service_method_table_t *service_method_table) override;

            virtual sai_status_t apiUninitialize(void) override;

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

        public: // notify syncd

            virtual sai_status_t notifySyncd(
                    _In_ sai_object_id_t switchId,
                    _In_ sai_redis_notify_syncd_t redisNotifySyncd) override;

        public:

            /**
             * @brief Checks whether attribute is custom SAI_REDIS_SWITCH attribute.
             *
             * This function should only be used on switch_api set function.
             */
            static bool isRedisAttribute(
                    _In_ sai_object_id_t obejctType,
                    _In_ const sai_attribute_t* attr);

            sai_status_t setRedisAttribute(
                    _In_ sai_object_id_t switchId,
                    _In_ const sai_attribute_t* attr);

            void setMeta(
                    _In_ std::weak_ptr<saimeta::Meta> meta);

            sai_switch_notifications_t syncProcessNotification(
                    _In_ std::shared_ptr<Notification> notification);

            const std::map<sai_object_id_t, swss::TableDump>& getTableDump() const;

            bool containsSwitch(
                    _In_ sai_object_id_t switchId) const;

        private: // QUAD API helpers

            sai_status_t create(
                    _In_ sai_object_type_t objectType,
                    _In_ const std::string& serializedObjectId,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

            sai_status_t remove(
                    _In_ sai_object_type_t objectType,
                    _In_ const std::string& serializedObjectId);

            sai_status_t set(
                    _In_ sai_object_type_t objectType,
                    _In_ const std::string& serializedObjectId,
                    _In_ const sai_attribute_t *attr);

            sai_status_t get(
                    _In_ sai_object_type_t objectType,
                    _In_ const std::string& serializedObjectId,
                    _In_ uint32_t attr_count,
                    _Inout_ sai_attribute_t *attr_list);

        private: // bulk QUAD API helpers

            sai_status_t bulkCreate(
                    _In_ sai_object_type_t object_type,
                    _In_ const std::vector<std::string> &serialized_object_ids,
                    _In_ const uint32_t *attr_count,
                    _In_ const sai_attribute_t **attr_list,
                    _In_ sai_bulk_op_error_mode_t mode,
                    _Inout_ sai_status_t *object_statuses);

            sai_status_t bulkRemove(
                    _In_ sai_object_type_t object_type,
                    _In_ const std::vector<std::string> &serialized_object_ids,
                    _In_ sai_bulk_op_error_mode_t mode,
                    _Out_ sai_status_t *object_statuses);

            sai_status_t bulkSet(
                    _In_ sai_object_type_t object_type,
                    _In_ const std::vector<std::string> &serialized_object_ids,
                    _In_ const sai_attribute_t *attr_list,
                    _In_ sai_bulk_op_error_mode_t mode,
                    _Out_ sai_status_t *object_statuses);

            sai_status_t bulkGet(
                    _In_ sai_object_type_t object_type,
                    _In_ const std::vector<std::string> &serialized_object_ids,
                    _In_ const uint32_t *attr_count,
                    _Inout_ sai_attribute_t **attr_list,
                    _In_ sai_bulk_op_error_mode_t mode,
                    _Out_ sai_status_t *object_statuses);

        private: // QUAD API response

            /**
             * @brief Wait for response.
             *
             * Will wait for response from syncd. Method used only for single
             * object create/remove/set since they have common output which is
             * sai_status_t.
             */
            sai_status_t waitForResponse(
                    _In_ sai_common_api_t api);

            /**
             * @brief Wait for GET response.
             *
             * Will wait for response from syncd. Method only used for single
             * GET object. If status is SUCCESS all values will be deserialized
             * and transferred to user buffers. If status is BUFFER_OVERFLOW
             * then all non list values will be transferred, but LIST objects
             * will only transfer COUNT item of list, without touching user
             * list at all.
             */
            sai_status_t waitForGetResponse(
                    _In_ sai_object_type_t objectType,
                    _In_ uint32_t attr_count,
                    _Inout_ sai_attribute_t *attr_list);

        private: // bulk QUAD API response

            /**
             * @brief Wait for bulk response.
             *
             * Will wait for response from syncd. Method used only for bulk
             * object create/remove/set since they have common output which is
             * sai_status_t and object_statuses.
             */
            sai_status_t waitForBulkResponse(
                    _In_ sai_common_api_t api,
                    _In_ uint32_t object_count,
                    _Out_ sai_status_t *object_statuses);

            /**
             * @brief Wait for bulk GET response.
             *
             * Will wait for response from syncd. Method only used for bulk
             * GET object. If object status is SUCCESS all values will be deserialized
             * and transferred to user buffers. If object status is BUFFER_OVERFLOW
             * then all non list values will be transferred, but LIST objects
             * will only transfer COUNT item of list, without touching user
             * list at all.
             */
            sai_status_t waitForBulkGetResponse(
                    _In_ sai_object_type_t objectType,
                    _In_ uint32_t object_count,
                    _In_ const uint32_t *attr_count,
                    _Inout_ sai_attribute_t **attr_list,
                    _Out_ sai_status_t *object_statuses);

        private: // stats API response

            sai_status_t waitForGetStatsResponse(
                    _In_ uint32_t number_of_counters,
                    _Out_ uint64_t *counters);

            sai_status_t waitForClearStatsResponse();

        private: // non QUAD API response

            sai_status_t waitForFlushFdbEntriesResponse();

        private: // SAI API response

            sai_status_t waitForQueryAttributeCapabilityResponse(
                    _Out_ sai_attr_capability_t* capability);

            sai_status_t waitForQueryAttributeEnumValuesCapabilityResponse(
                    _Inout_ sai_s32_list_t* enumValuesCapability);

            sai_status_t waitForObjectTypeGetAvailabilityResponse(
                    _In_ uint64_t *count);

            sai_status_t waitForQueryStatsCapabilityResponse(
                    _Inout_ sai_stat_capability_list_t* stats_capability);

        private: // notify syncd response

            sai_status_t waitForNotifySyncdResponse();

        private: // notification

            void notificationThreadFunction();

            void handleNotification(
                    _In_ const std::string &name,
                    _In_ const std::string &serializedNotification,
                    _In_ const std::vector<swss::FieldValueTuple> &values);

            sai_status_t setRedisExtensionAttribute(
                    _In_ sai_object_type_t objectType,
                    _In_ sai_object_id_t objectId,
                    _In_ const sai_attribute_t *attr);

            bool isSaiS8ListValidString(
                    _In_ const sai_s8_list_t &s8list);

            bool emplaceStrings(
                    _In_ const sai_s8_list_t &field,
                    _In_ const sai_s8_list_t &value,
                    _Out_ std::vector<swss::FieldValueTuple> &entries);

            bool emplaceStrings(
                    _In_ const char *field,
                    _In_ const sai_s8_list_t &value,
                    _Out_ std::vector<swss::FieldValueTuple> &entries);

            sai_status_t notifyCounterGroupOperations(
                    _In_ sai_object_id_t objectId,
                    _In_ const sai_redis_flex_counter_group_parameter_t *flexCounterGroupParam);

            sai_status_t notifyCounterOperations(
                    _In_ sai_object_id_t objectId,
                    _In_ const sai_redis_flex_counter_parameter_t *flexCounterParam);

        private:

            sai_status_t sai_redis_notify_syncd(
                    _In_ sai_object_id_t switchId,
                    _In_ const sai_attribute_t *attr);

            void clear_local_state();

            sai_switch_notifications_t processNotification(
                    _In_ std::shared_ptr<Notification> notification);

            void refreshTableDump();

        private:

            std::shared_ptr<ContextConfig> m_contextConfig;

            bool m_asicInitViewMode;

            bool m_useTempView;

            bool m_syncMode;

            sai_redis_communication_mode_t m_redisCommunicationMode;

            bool m_initialized;

            std::shared_ptr<Recorder> m_recorder;

            std::shared_ptr<SwitchContainer> m_switchContainer;

            std::shared_ptr<VirtualObjectIdManager> m_virtualObjectIdManager;

            std::shared_ptr<swss::DBConnector> m_db;

            std::shared_ptr<RedisVidIndexGenerator> m_redisVidIndexGenerator;

            std::weak_ptr<saimeta::Meta> m_meta;

            std::shared_ptr<SkipRecordAttrContainer> m_skipRecordAttrContainer;

            std::shared_ptr<Channel> m_communicationChannel;

            uint64_t m_responseTimeoutMs;

            std::function<sai_switch_notifications_t(std::shared_ptr<Notification>)> m_notificationCallback;

            std::map<sai_object_id_t, swss::TableDump> m_tableDump;
    };
}
