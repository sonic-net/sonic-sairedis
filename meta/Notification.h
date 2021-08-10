#pragma once

extern "C" {
#include "saimetadata.h"
}

#include "Meta.h"

#include <string>
#include <memory>

namespace sairedis
{
    class Notification
    {
        public:

            Notification(
                    _In_ sai_switch_notification_type_t switchNotificationType,
                    _In_ const std::string& serializedNotification);

            virtual ~Notification() = default;

        public:

            /**
             * @brief Return switch ID for notification.
             *
             * If notification contains switch id field, then returns value of
             * that field. If notification contains multiple switch id fields,
             * first one is returned.
             *
             * If notification don't contain switch id field, return value is
             * SAI_NULL_OBJECT_ID.
             */
            virtual sai_object_id_t getSwitchId() const = 0;

            /**
             * @brief Get any object id.
             *
             * Some notifications may not contain any switch id field, like for
             * example queue_deadlock_notification, then any object id defined
             * on that notification will be returned.
             *
             * If notification defines switch if object and is not NULL then it
             * should be returned.
             *
             * This object id will be used to determine switch id using
             * sai_switch_id_query() API.
             *
             * If no other object besides switch id is defined, then this
             * function returns switch id.
             */
            virtual sai_object_id_t getAnyObjectId() const = 0;

            /**
             * @brief Process metadata for notification.
             *
             * Metadata database must be aware of each notifications when they
             * arrive, this will pass notification data to notification
             * function.
             *
             * This function must be executed under sairedis API mutex.
             */
            virtual void processMetadata(
                    std::shared_ptr<saimeta::Meta> meta) const = 0;

            /**
             * @brief Execute callback notification.
             *
             * Execute callback notification on right notification pointer passing
             * deserialized data.
             */
            virtual void executeCallback(
                    _In_ const sai_switch_notifications_t& switchNotifications) const = 0;

        public:

            /**
             * @brief Get notification type.
             */
            sai_switch_notification_type_t getNotificationType() const;

            /**
             * @brief Get serialized notification.
             *
             * Contains all notification data without notification name.
             */
            const std::string& getSerializedNotification() const;

        private:

            const sai_switch_notification_type_t m_switchNotificationType;

            const std::string m_serializedNotification;
    };
}
