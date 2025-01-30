#pragma once

extern "C"{
#include "otaimetadata.h"
}

#include "NotificationQueue.h"
#include "NotificationProcessor.h"

#include "swss/table.h"

#include <string>
#include <vector>
#include <memory>

namespace syncd
{
    class NotificationHandler
    {
        public:

            NotificationHandler(
                    _In_ std::shared_ptr<NotificationProcessor> processor,
                    _In_ otai_api_version_t apiVersion = OTAI_VERSION(0,0,0));

            virtual ~NotificationHandler();

        public:

            void setSwitchNotifications(
                    _In_ const otai_switch_notifications_t& switchNotifications);

            const otai_switch_notifications_t& getSwitchNotifications() const;

            void updateNotificationsPointers(
                    _In_ uint32_t attr_count,
                    _In_ otai_attribute_t *attr_list) const;

            void setApiVersion(
                    _In_ otai_api_version_t apiVersion);

            otai_api_version_t getApiVersion() const;

        public: // members reflecting OTAI callbacks

            void onFdbEvent(
                    _In_ uint32_t count,
                    _In_ const otai_fdb_event_notification_data_t *data);

            void onNatEvent(
                    _In_ uint32_t count,
                    _In_ const otai_nat_event_notification_data_t *data);

            void onPortStateChange(
                    _In_ uint32_t count,
                    _In_ const otai_port_oper_status_notification_t *data);

            void onPortHostTxReady(
                    _In_ otai_object_id_t switch_id,
                    _In_ otai_object_id_t port_id,
                    _In_ otai_port_host_tx_ready_status_t host_tx_ready_status);

            void onQueuePfcDeadlock(
                    _In_ uint32_t count,
                    _In_ const otai_queue_deadlock_notification_data_t *data);

            void onSwitchAsicSdkHealthEvent(
                    _In_ otai_object_id_t switch_id,
                    _In_ otai_switch_asic_sdk_health_severity_t severity,
                    _In_ otai_timespec_t timestamp,
                    _In_ otai_switch_asic_sdk_health_category_t category,
                    _In_ otai_switch_health_data_t data,
                    _In_ const otai_u8_list_t description);

            void onSwitchShutdownRequest(
                    _In_ otai_object_id_t switch_id);

            void onSwitchStateChange(
                    _In_ otai_object_id_t switch_id,
                    _In_ otai_switch_oper_status_t switch_oper_status);

            void onBfdSessionStateChange(
                    _In_ uint32_t count,
                    _In_ const otai_bfd_session_state_notification_t *data);

            void onTwampSessionEvent(
                    _In_ uint32_t count,
                    _In_ const otai_twamp_session_event_notification_data_t *data);

        private:

            void enqueueNotification(
                    _In_ const std::string& op,
                    _In_ const std::string& data,
                    _In_ const std::vector<swss::FieldValueTuple> &entry);

            void enqueueNotification(
                    _In_ const std::string& op,
                    _In_ const std::string& data);

        private:

            otai_switch_notifications_t m_switchNotifications;

            std::shared_ptr<NotificationQueue> m_notificationQueue;

            std::shared_ptr<NotificationProcessor> m_processor;

            otai_api_version_t m_apiVersion;
    };
}
