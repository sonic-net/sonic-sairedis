#pragma once

#include "Notification.h"

namespace sairedis
{
    class NotificationSwitchAsicSdkHealthEvent:
        public Notification
    {
        public:

            NotificationSwitchAsicSdkHealthEvent(
                    _In_ const std::string& serializedNotification);

            virtual ~NotificationSwitchAsicSdkHealthEvent();

        public:

            virtual sai_object_id_t getSwitchId() const override;

            virtual sai_object_id_t getAnyObjectId() const override;

            virtual void processMetadata(
                    _In_ std::shared_ptr<saimeta::Meta> meta) const override;

            virtual void executeCallback(
                    _In_ const sai_switch_notifications_t& switchNotifications) const override;

        private:

            sai_object_id_t m_switchId;
            sai_switch_asic_sdk_health_severity_t m_severity;
            sai_switch_asic_sdk_health_category_t m_category;
            sai_timespec_t m_timestamp;
            sai_switch_health_data_t m_healthData;
            sai_u8_list_t m_description;
    };
}
