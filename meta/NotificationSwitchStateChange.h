#pragma once

#include "Notification.h"

namespace sairedis
{
    class NotificationSwitchStateChange:
        public Notification
    {
        public:

            NotificationSwitchStateChange(
                    _In_ const std::string& serializedNotification);

            virtual ~NotificationSwitchStateChange() = default;

        public:

            virtual sai_object_id_t getSwitchId() const override;

            virtual sai_object_id_t getAnyObjectId() const override;

            virtual void processMetadata(
                    _In_ std::shared_ptr<saimeta::Meta> meta) const override;

            virtual void executeCallback(
                    _In_ const sai_switch_notifications_t& switchNotifications) const override;

        private:

            sai_object_id_t m_switchId;

            sai_switch_oper_status_t m_switchOperStatus;
    };
}
