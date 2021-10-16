#pragma once

extern "C"{
#include "saimetadata.h"
}

#include "EventPayload.h"

#include "meta/Notification.h"

namespace saivs
{
    class EventPayloadNotification:
        public EventPayload
    {
        public:

            EventPayloadNotification(
                    _In_ std::shared_ptr<sairedis::Notification> ntf,
                    _In_ const sai_switch_notifications_t& switchNotifications);

            virtual ~EventPayloadNotification() = default;

        public:

            std::shared_ptr<sairedis::Notification> getNotification() const;

            const sai_switch_notifications_t& getSwitchNotifications() const;

        private:

            std::shared_ptr<sairedis::Notification> m_ntf;

            sai_switch_notifications_t m_switchNotifications;
    };
}
