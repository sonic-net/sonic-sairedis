#pragma once

#include "OtaiInterface.h"

#include <memory>
#include <thread>
#include <mutex>
#include <queue>

namespace otaimeta
{
    /**
     * @brief Dummy OTAI interface.
     *
     * Defined for unittests.
     */
    class DummyOtaiInterface:
        public otairedis::OtaiInterface
    {
        public:

            DummyOtaiInterface();

            virtual ~DummyOtaiInterface();

        public:

            void setStatus(
                    _In_ otai_status_t status);

        public:

            virtual otai_status_t apiInitialize(
                    _In_ uint64_t flags,
                    _In_ const otai_service_method_table_t *service_method_table) override;

            virtual otai_status_t apiUninitialize(void) override;

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

           virtual otai_status_t switchMdioRead(
                   _In_ otai_object_id_t switchId,
                   _In_ uint32_t device_addr,
                   _In_ uint32_t start_reg_addr,
                   _In_ uint32_t number_of_registers,
                   _Out_ uint32_t *reg_val) override;

           virtual otai_status_t switchMdioWrite(
                   _In_ otai_object_id_t switchId,
                   _In_ uint32_t device_addr,
                   _In_ uint32_t start_reg_addr,
                   _In_ uint32_t number_of_registers,
                   _In_ const uint32_t *reg_val) override;

           virtual otai_status_t switchMdioCl22Read(
                   _In_ otai_object_id_t switchId,
                   _In_ uint32_t device_addr,
                   _In_ uint32_t start_reg_addr,
                   _In_ uint32_t number_of_registers,
                   _Out_ uint32_t *reg_val) override;

           virtual otai_status_t switchMdioCl22Write(
                   _In_ otai_object_id_t switch_id,
                   _In_ uint32_t device_addr,
                   _In_ uint32_t start_reg_addr,
                   _In_ uint32_t number_of_registers,
                   _In_ const uint32_t *reg_val) override;

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

//            virtual otai_status_t queryApiVersion(
//                    _Out_ otai_api_version_t *version) override;

        protected:

            void updateNotificationPointers(
                    _In_ uint32_t count,
                    _In_ const otai_attribute_t* attrs);

        public:

            /**
             * @brief Will start sending notifications
             */
            otai_status_t start();

            /**
             * @brief Will stop sending notifications
             */
            otai_status_t stop();

            /**
             * @brief Enqueue notification to send. Will send specific dummy
             * notification from notifications thread.
             */

            otai_status_t enqueueNotificationToSend(
                    _In_ otai_attr_id_t id);

        protected:

            /**
             * @brief Try get notification to send.
             *
             * If notification queue is not empty, it will return true and
             * set id to attribute of notification.
             */
            bool tryGetNotificationToSend(
                    _Out_ otai_attr_id_t& id);

            /**
             * @brief Send notification.
             *
             * Will actually send notification if expected pointer for
             * notification is not null.
             */
            void sendNotification(
                    _In_ otai_attr_id_t id);

        protected:

            void run();

        protected:

            otai_linecard_notifications_t m_sn;

            otai_status_t m_status;

            bool m_apiInitialized;

            /**
             * @brief Thread that will be used to send notifications
             */
            std::shared_ptr<std::thread> m_thread;

            std::mutex m_mutex;

            bool m_runThread;

            std::queue<otai_attr_id_t> m_queue;
    };
}
