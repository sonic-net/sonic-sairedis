#pragma once

#include "NotificationQueue.h"
#include "VirtualOidTranslator.h"
#include "RedisClient.h"
#include "NotificationProducerBase.h"

#include "swss/notificationproducer.h"

#include <thread>
#include <memory>
#include <condition_variable>
#include <functional>

namespace syncd
{
    class NotificationProcessor
    {
        public:

            NotificationProcessor(
                    _In_ std::shared_ptr<NotificationProducerBase> producer,
                    _In_ std::shared_ptr<RedisClient> client,
                    _In_ std::function<void(const swss::KeyOpFieldsValuesTuple&)> synchronizer);

            virtual ~NotificationProcessor();

        public:

            std::shared_ptr<NotificationQueue> getQueue() const;

            void signal();

            void startNotificationsProcessingThread();

            void stopNotificationsProcessingThread();

        private:

            void ntf_process_function();

            void sendNotification(
                    _In_ const std::string& op,
                    _In_ const std::string& data,
                    _In_ std::vector<swss::FieldValueTuple> entry);

            void sendNotification(
                    _In_ const std::string& op,
                    _In_ const std::string& data);

            sai_fdb_entry_type_t getFdbEntryType(
                    _In_ uint32_t count,
                    _In_ const sai_attribute_t *list);

            void redisPutFdbEntryToAsicView(
                    _In_ const sai_fdb_event_notification_data_t *fdb);

            bool check_fdb_event_notification_data(
                    _In_ const sai_fdb_event_notification_data_t& data);

            bool check_nat_event_notification_data(
                    _In_ const sai_nat_event_notification_data_t& data);

            bool contains_fdb_flush_event(
                    _In_ uint32_t count,
                    _In_ const sai_fdb_event_notification_data_t *data);

        private: // processors

            void process_on_switch_state_change(
                    _In_ sai_object_id_t switch_rid,
                    _In_ sai_switch_oper_status_t switch_oper_status);


            void process_on_fdb_event(
                    _In_ uint32_t count,
                    _In_ sai_fdb_event_notification_data_t *data);

            void process_on_nat_event(
                    _In_ uint32_t count,
                    _In_ sai_nat_event_notification_data_t *data);

            void process_on_queue_deadlock_event(
                    _In_ uint32_t count,
                    _In_ sai_queue_deadlock_notification_data_t *data);

            void process_on_port_state_change(
                    _In_ uint32_t count,
                    _In_ sai_port_oper_status_notification_t *data);

            void process_on_bfd_session_state_change(
                    _In_ uint32_t count,
                    _In_ sai_bfd_session_state_notification_t *data);

            void process_on_port_host_tx_ready_change(
                    _In_ sai_object_id_t switch_id,
                    _In_ sai_object_id_t port_id,
                    _In_ sai_port_host_tx_ready_status_t *host_tx_ready_status);

            void process_on_switch_asic_sdk_health_event(
                    _In_ sai_object_id_t switch_id,
                    _In_ sai_switch_asic_sdk_health_severity_t severity,
                    _In_ sai_timespec_t timestamp,
                    _In_ sai_switch_asic_sdk_health_category_t category,
                    _In_ sai_switch_health_data_t data,
                    _In_ const sai_u8_list_t description);

            void process_on_switch_shutdown_request(
                    _In_ sai_object_id_t switch_rid);

            void process_on_twamp_session_event(
                    _In_ uint32_t count,
                    _In_ sai_twamp_session_event_notification_data_t *data);

        private: // handlers

            void handle_switch_state_change(
                    _In_ const std::string &data);

            void handle_fdb_event(
                    _In_ const std::string &data);

            void handle_nat_event(
                    _In_ const std::string &data);

            void handle_queue_deadlock(
                    _In_ const std::string &data);

            void handle_port_state_change(
                    _In_ const std::string &data);

            void handle_bfd_session_state_change(
                    _In_ const std::string &data);

            void handle_switch_asic_sdk_health_event(
                    _In_ const std::string &data);

            void handle_switch_shutdown_request(
                    _In_ const std::string &data);

            void handle_port_host_tx_ready_change(
                    _In_ const std::string &data);

            void handle_twamp_session_event(
                    _In_ const std::string &data);

            void handle_tam_tel_type_config_change(
                    _In_ const std::string &data);

            void processNotification(
                    _In_ const swss::KeyOpFieldsValuesTuple& item);

        public:

            void syncProcessNotification(
                    _In_ const swss::KeyOpFieldsValuesTuple& item);

        public: // TODO to private

            std::shared_ptr<VirtualOidTranslator> m_translator;

        private:

            std::shared_ptr<NotificationQueue> m_notificationQueue;

            std::shared_ptr<std::thread> m_ntf_process_thread;

            // condition variable will be used to notify processing thread
            // that some notification arrived

            std::condition_variable m_cv;

            // determine whether notification thread is running

            bool m_runThread;

            std::function<void(const swss::KeyOpFieldsValuesTuple&)> m_synchronizer;

            std::shared_ptr<RedisClient> m_client;

            std::shared_ptr<NotificationProducerBase> m_notifications;
    };
}
