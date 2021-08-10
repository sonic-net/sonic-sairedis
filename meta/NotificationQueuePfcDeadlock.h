#pragma once

#include "Notification.h"

namespace sairedis
{
    class NotificationQueuePfcDeadlock:
        public Notification
    {
        public:

            NotificationQueuePfcDeadlock(
                    _In_ const std::string& serializedNotification);

            virtual ~NotificationQueuePfcDeadlock();

        public:

            virtual sai_object_id_t getSwitchId() const override;

            virtual sai_object_id_t getAnyObjectId() const override;

            virtual void processMetadata(
                    _In_ std::shared_ptr<saimeta::Meta> meta) const override;

            virtual void executeCallback(
                    _In_ const sai_switch_notifications_t& switchNotifications) const override;

        private:

            uint32_t m_count;

            sai_queue_deadlock_notification_data_t *m_queueDeadlockNotificationData;
    };
}

