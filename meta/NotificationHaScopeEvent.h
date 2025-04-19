#pragma once

#include "Notification.h"

namespace sairedis
{
    class NotificationHaScopeEvent:
        public Notification
    {
        public:

            NotificationHaScopeEvent(
                    _In_ const std::string& serializedNotification);

            virtual ~NotificationHaScopeEvent();

        public:

            virtual sai_object_id_t getSwitchId() const override;

            virtual sai_object_id_t getAnyObjectId() const override;

            virtual void processMetadata(
                    _In_ std::shared_ptr<saimeta::Meta> meta) const override;

            virtual void executeCallback(
                    _In_ const sai_switch_notifications_t& switchNotifications) const override;

        private:

            uint32_t m_count;

            sai_ha_scope_event_data_t* m_haScopeEventData;
    };
}
