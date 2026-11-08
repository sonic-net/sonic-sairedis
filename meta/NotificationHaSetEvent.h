#pragma once

#include "Notification.h"

namespace sairedis
{
    class NotificationHaSetEvent:
        public Notification
    {
        public:

            NotificationHaSetEvent(
                    _In_ const std::string& serializedNotification);

            virtual ~NotificationHaSetEvent();

        public:

            virtual sai_object_id_t getSwitchId() const override;

            virtual sai_object_id_t getAnyObjectId() const override;

            virtual void processMetadata(
                    _In_ std::shared_ptr<saimeta::Meta> meta) const override;

            virtual void executeCallback(
                    _In_ const sai_switch_notifications_t& switchNotifications) const override;

        private:

            uint32_t m_count;

            sai_ha_set_event_data_t* m_haSetEventData;
    };
}
