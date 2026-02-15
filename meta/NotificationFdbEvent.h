#pragma once

#include "Notification.h"

namespace sairedis
{
    class NotificationFdbEvent:
        public Notification
    {
        public:

            NotificationFdbEvent(
                    _In_ const std::string& serializedNotification);

            virtual ~NotificationFdbEvent();

        public:

            virtual sai_object_id_t getSwitchId() const override;

            virtual sai_object_id_t getAnyObjectId() const override;

            virtual void processMetadata(
                    _In_ std::shared_ptr<saimeta::Meta> meta) const override;

            virtual void executeCallback(
                    _In_ const sai_switch_notifications_t& switchNotifications) const override;

        private:

            uint32_t m_count;

            sai_fdb_event_notification_data_t* m_fdbEventNotificationData;
    };
}
