#pragma once

#include "swss/selectableevent.h"

#include "meta/SaiInterface.h"
#include "meta/SelectableChannel.h"

#include "syncd/ServiceMethodTable.h"
#include "syncd/NotificationProducerBase.h"
#include "syncd/SwitchNotifications.h"

#include "Options.h"

#include <map>
#include <memory>
#include <thread>
#include <string>

namespace saiproxy
{
    class Proxy
    {
        public:

            Proxy(
                    _In_ std::shared_ptr<sairedis::SaiInterface> vendorSai);

            Proxy(
                    _In_ std::shared_ptr<sairedis::SaiInterface> vendorSai,
                    _In_ std::shared_ptr<Options> options);

            virtual ~Proxy();

        public:

            void run();

            void stop();

        private:

            const char* profileGetValue(
                    _In_ sai_switch_profile_id_t profile_id,
                    _In_ const char* variable);

            int profileGetNextValue(
                    _In_ sai_switch_profile_id_t profile_id,
                    _Out_ const char** variable,
                    _Out_ const char** value);

            void processEvent(
                    _In_ sairedis::SelectableChannel& consumer);

            void processSingleEvent(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

        private: // api process methods

            // TODO implement bulk apis

            void processCreate(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            void processRemove(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            void processSet(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            void processGet(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            void processCreateEntry(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            void processFlushFdbEntries(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            void processObjectTypeGetAvailability(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            void processQueryAttributeCapability(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            void processQueryAttributeEnumValuesCapability(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            void processObjectTypeQuery(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            void processSwitchIdQuery(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            void processQueryApiVersion(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            void processLogSet(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            void processGetStats(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            void processGetStatsExt(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            void processClearStats(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

        private: // notifications

            void onFdbEvent(
                    _In_ uint32_t count,
                    _In_ const sai_fdb_event_notification_data_t *data);

            void onNatEvent(
                    _In_ uint32_t count,
                    _In_ const sai_nat_event_notification_data_t *data);

            void onPortStateChange(
                    _In_ uint32_t count,
                    _In_ const sai_port_oper_status_notification_t *data);

            void onPortHostTxReady(
                    _In_ sai_object_id_t switch_id,
                    _In_ sai_object_id_t port_id,
                    _In_ sai_port_host_tx_ready_status_t host_tx_ready_status);

            void onQueuePfcDeadlock(
                    _In_ uint32_t count,
                    _In_ const sai_queue_deadlock_notification_data_t *data);

            void onSwitchShutdownRequest(
                    _In_ sai_object_id_t switch_id);

            void onSwitchAsicSdkHealthEvent(
                    _In_ sai_object_id_t switch_id,
                    _In_ sai_switch_asic_sdk_health_severity_t severity,
                    _In_ sai_timespec_t timestamp,
                    _In_ sai_switch_asic_sdk_health_category_t category,
                    _In_ sai_switch_health_data_t data,
                    _In_ const sai_u8_list_t description);

            void onSwitchStateChange(
                    _In_ sai_object_id_t switch_id,
                    _In_ sai_switch_oper_status_t switch_oper_status);

            void onBfdSessionStateChange(
                    _In_ uint32_t count,
                    _In_ const sai_bfd_session_state_notification_t *data);

            void onTwampSessionEvent(
                    _In_ uint32_t count,
                    _In_ const sai_twamp_session_event_notification_data_t *data);

            void onTamTelTypeConfigChange(
                    _In_ sai_object_id_t tam_tel_id);

            void sendNotification(
                    _In_ const std::string& op,
                    _In_ const std::string& data);

        private:

            void loadProfileMap();

            void updateAttributteNotificationPointers(
                    _In_ uint32_t count,
                    _Inout_ sai_attribute_t* attr_list);

        public:

            uint64_t getNotificationsSentCount() const;

        private:

            syncd::ServiceMethodTable m_smt;

            sai_service_method_table_t m_test_services;

            std::shared_ptr<sairedis::SaiInterface> m_vendorSai;

            swss::SelectableEvent m_stopEvent;

            std::map<std::string, std::string> m_profileMap;

            std::map<std::string, std::string>::iterator m_profileIter;

            std::shared_ptr<sairedis::SelectableChannel> m_selectableChannel;

            std::shared_ptr<syncd::NotificationProducerBase> m_notifications;

            std::shared_ptr<Options> m_options;

            /**
             * @brief Mutex for synchronizing api execution and notifications
             */
            std::mutex m_mutex;

            bool m_apiInitialized;

            sai_switch_notifications_t m_sn;

            syncd::SwitchNotifications m_swNtf;

            /**
             * @brief Notifications sent count.
             *
             * This value can be used to write unittests when testing
             * notifications.
             */
            uint64_t m_notificationsSentCount;

            sai_api_version_t m_apiVersion;
    };
}
