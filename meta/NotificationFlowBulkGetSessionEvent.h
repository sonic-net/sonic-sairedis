#pragma once

#include "Notification.h"

namespace sairedis
{
    class NotificationFlowBulkGetSessionEvent:
        public Notification
    {
        public:

            NotificationFlowBulkGetSessionEvent(
                    _In_ const std::string& serializedNotification);

            virtual ~NotificationFlowBulkGetSessionEvent();

        public:

            virtual sai_object_id_t getSwitchId() const override;

            virtual sai_object_id_t getAnyObjectId() const override;

            virtual void processMetadata(
                    _In_ std::shared_ptr<saimeta::Meta> meta) const override;

            virtual void executeCallback(
                    _In_ const sai_switch_notifications_t& switchNotifications) const override;

        private:

            sai_object_id_t m_flow_bulk_session_id;

            uint32_t m_count;

            sai_flow_bulk_get_session_event_data_t* m_data;
    };
}

